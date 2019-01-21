/*
 * Copyright (c) 2019 Dmitry Marakasov <amdmi3@amdmi3.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "jsonslicer.hh"

#include "handlers.hh"

#include <Python.h>
#include <yajl/yajl_parse.h>

#include <new>

PyObject* JsonSlicer_new(PyTypeObject* type, PyObject*, PyObject*) {
	JsonSlicer* self = (JsonSlicer*)type->tp_alloc(type, 0);
	if (self != nullptr) {
		new(&self->io) PyObjPtr();
		self->read_size = 1024;  // XXX: bump somewhat for production use
		self->path_mode = JsonSlicer::PathMode::IGNORE;

		self->yajl = nullptr;

		new(&self->last_map_key) PyObjPtr();
		self->state = JsonSlicer::State::SEEKING;

		self->pattern.init();
		self->path.init();
		self->constructing.init();
		self->complete.init();
	}
	return (PyObject*)self;
}

void JsonSlicer_dealloc(JsonSlicer* self) {
	self->complete.clear();
	self->constructing.clear();
	self->path.clear();
	self->pattern.clear();

	self->last_map_key.~PyObjPtr();

	if (self->yajl != nullptr) {
		yajl_handle tmp = self->yajl;
		self->yajl = nullptr;
		yajl_free(tmp);
	}
	self->io.~PyObjPtr();

	Py_TYPE(self)->tp_free((PyObject*)self);
}

int JsonSlicer_init(JsonSlicer* self, PyObject* args, PyObject* kwargs) {
	// parse args
	PyObject* io = nullptr;
	PyObject* pattern = nullptr;
	Py_ssize_t read_size = self->read_size;
	JsonSlicer::PathMode path_mode = self->path_mode;
	int enable_yajl_allow_comments = false;
	int enable_yajl_dont_validate_strings = false;
	int enable_yajl_allow_trailing_garbage = false;
	int enable_yajl_allow_multiple_values = false;
	int enable_yajl_allow_partial_values = false;

	static const char* keywords[] = {
		"file",
		"path_prefix",
		"read_size",
		"path_mode",
		"yajl_allow_comments",
		"yajl_dont_validate_strings",
		"yajl_allow_trailing_garbage",
		"yajl_allow_multiple_values",
		"yajl_allow_partial_values",
		nullptr
	};

	const char* path_mode_arg = nullptr;
	if (!PyArg_ParseTupleAndKeywords(
				args, kwargs, "OO|$nsppppp", const_cast<char**>(keywords),
				&io,
				&pattern,
				&read_size,
				&path_mode_arg,
				&enable_yajl_allow_comments,
				&enable_yajl_dont_validate_strings,
				&enable_yajl_allow_trailing_garbage,
				&enable_yajl_allow_multiple_values,
				&enable_yajl_allow_partial_values)) {
		return -1;
	}

	if (path_mode_arg) {
		if (strcmp(path_mode_arg, "ignore") == 0) {
			path_mode = JsonSlicer::PathMode::IGNORE;
		} else if (strcmp(path_mode_arg, "map_keys") == 0) {
			path_mode = JsonSlicer::PathMode::MAP_KEYS;
		} else if (strcmp(path_mode_arg, "full") == 0) {
			path_mode = JsonSlicer::PathMode::FULL;
		} else {
			PyErr_SetString(PyExc_ValueError, "Bad value for path_mode argument");
			return -1;
		}
	}

	assert(io != nullptr);
	assert(pattern != nullptr);

	// prepare all new data members
	PyObjList new_pattern;
	new_pattern.init();

	for (Py_ssize_t i = 0; i < PySequence_Size(pattern); i++) {
		PyObjPtr item = PyObjPtr::Take(PySequence_GetItem(pattern, i));
		if (!item.valid()) {
			new_pattern.clear();
			return -1;
		}
		if (!new_pattern.push_back(item)) {
			new_pattern.clear();
			return -1;
		}
	}

	yajl_handle new_yajl = yajl_alloc(&yajl_handlers, nullptr, (void*)self);
	if (new_yajl == nullptr) {
		new_pattern.clear();
		PyErr_SetString(PyExc_RuntimeError, "Cannot allocate YAJL handle");
		return -1;
	}

	if (enable_yajl_allow_comments && yajl_config(new_yajl, yajl_allow_comments, 1) == 0) {
		yajl_free(new_yajl);
		new_pattern.clear();
		PyErr_SetString(PyExc_RuntimeError, "Cannot set yajl_allow_comments");
		return -1;
	}
	if (enable_yajl_dont_validate_strings && yajl_config(new_yajl, yajl_dont_validate_strings, 1) == 0) {
		yajl_free(new_yajl);
		new_pattern.clear();
		PyErr_SetString(PyExc_RuntimeError, "Cannot set yajl_dont_validate_strings");
		return -1;
	}
	if (enable_yajl_allow_trailing_garbage && yajl_config(new_yajl, yajl_allow_trailing_garbage, 1) == 0) {
		yajl_free(new_yajl);
		new_pattern.clear();
		PyErr_SetString(PyExc_RuntimeError, "Cannot set yajl_allow_trailing_garbage");
		return -1;
	}
	if (enable_yajl_allow_multiple_values && yajl_config(new_yajl, yajl_allow_multiple_values, 1) == 0) {
		yajl_free(new_yajl);
		new_pattern.clear();
		PyErr_SetString(PyExc_RuntimeError, "Cannot set yajl_allow_multiple_values");
		return -1;
	}
	if (enable_yajl_allow_partial_values && yajl_config(new_yajl, yajl_allow_partial_values, 1) == 0) {
		yajl_free(new_yajl);
		new_pattern.clear();
		PyErr_SetString(PyExc_RuntimeError, "Cannot set yajl_allow_partial_values");
		return -1;
	}

	// swap initialized members with new ones, clearing the rest
	self->complete.clear();

	self->constructing.clear();

	self->path.clear();

	{
		self->pattern.swap(new_pattern);
		new_pattern.clear();
	}

	self->state = JsonSlicer::State::SEEKING;

	self->last_map_key.~PyObjPtr();

	{
		yajl_handle tmp = self->yajl;
		self->yajl = new_yajl;
		if (tmp != nullptr) {
			yajl_free(tmp);
		}
	}

	self->path_mode = path_mode;
	self->read_size = read_size;

	self->io = PyObjPtr::Borrow(io);

	return 0;
}
