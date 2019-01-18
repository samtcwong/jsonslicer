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

#ifndef JSONSLICER_JSONSLICER_H
#define JSONSLICER_JSONSLICER_H

#include "pyobjlist.hh"

#include <Python.h>
#include <yajl/yajl_parse.h>

enum JsonSlicerMode {
	MODE_SEEKING,
	MODE_CONSTRUCTING
};

enum JsonSlicerPathMode {
	PATHMODE_IGNORE,
	PATHMODE_MAP_KEYS,
	PATHMODE_FULL,
};

typedef struct {
	PyObject_HEAD

	// arguments
	PyObject* io;
	Py_ssize_t read_size;
	int path_mode;

	// YAJL handle
	yajl_handle yajl;

	// parser state
	PyObject* last_map_key;
	int mode;

	// pattern argument
	PyObjList pattern;

	// current path in json
	PyObjList path;

	// stack of objects being currently constructed
	PyObjList constructing;

	// complete python objects ready to be returned to caller
	PyObjList complete;
} JsonSlicer;

PyObject* JsonSlicer_new(PyTypeObject *type, PyObject *args, PyObject *kwds);
void JsonSlicer_dealloc(JsonSlicer* self);
int JsonSlicer_init(JsonSlicer* self, PyObject* args, PyObject* kwargs);

JsonSlicer* JsonSlicer_iter(JsonSlicer* self);
PyObject* JsonSlicer_iternext(JsonSlicer* self);

extern PyTypeObject JsonSlicerType;

#endif