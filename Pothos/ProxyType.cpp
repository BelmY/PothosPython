// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "PothosModule.hpp"
#include <cassert>

static PyTypeObject ProxyType = {
    PyObject_HEAD_INIT(NULL)
};

static PyNumberMethods ProxyNumberMethods = {
};

static void Proxy_dealloc(ProxyObject *self)
{
    delete self->proxy;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static int Proxy_init(ProxyObject *self, PyObject *args, PyObject *)
{
    //check the input
    if (args != nullptr and PyTuple_Size(args) > 1)
    {
        PyErr_SetString(PyExc_RuntimeError, "Proxy __init__ takes one or zero args");
        return -1;
    }

    //allocate the proxy container
    self->proxy = new Pothos::Proxy();

    //arg0 was specified, make a proxy from py object
    if (args != nullptr and PyTuple_Size(args) > 0)
    {
        (*self->proxy) = PyObjectToProxy(PyTuple_GetItem(args, 0));
    }

    return 0;
}

static PyObject* Proxy_getattr(PyObject *self, PyObject *attr_name)
{
    auto callable = PyObject_GenericGetAttr(self, attr_name);
    if (callable != nullptr) return callable;

    PyErr_Clear(); //PyObject_GenericGetAttr sets an error when not found

    try
    {
        //extract string name
        const auto name = PyObjectToProxy(attr_name).convert<std::string>();

        Pothos::Proxy proxy;
        {
            PyThreadStateLock lock; //proxy call could be potentially blocking
            proxy = reinterpret_cast<ProxyObject *>(self)->proxy->get(name);
        }

        //convert the result into a pyobject
        return ProxyToPyObject(proxyEnvTranslate(proxy, getPythonProxyEnv()));
    }
    catch (const Pothos::Exception &) {}

    auto args = PyObjectRef(PyTuple_Pack(2, self, attr_name), REF_NEW);
    return makeProxyCallObject(args.obj);
}

int Proxy_setattr(PyObject *self, PyObject *attr_name, PyObject *v)
{
    int r = PyObject_GenericSetAttr(self, attr_name, v);
    if (r == -1)
    {
        PyErr_Clear(); //PyObject_GenericSetAttr sets an error when not found
    }

    try
    {
        //extract string name
        const auto name = PyObjectToProxy(attr_name).convert<std::string>();

        //extract the value
        const auto value = PyObjectToProxy(v);

        {
            PyThreadStateLock lock; //proxy call could be potentially blocking
            reinterpret_cast<ProxyObject *>(self)->proxy->set(name, value);
        }
    }
    catch (const Pothos::Exception &ex)
    {
        PyErr_SetString(PyExc_RuntimeError, ex.displayText().c_str());
        return -1;
    }

    return 0;
}

static PyObject *Proxy_convert(ProxyObject *self)
{
    try
    {
        return ProxyToPyObject(proxyEnvTranslate(*self->proxy, getPythonProxyEnv()));
    }
    catch (const Pothos::Exception &ex)
    {
        PyErr_SetString(PyExc_RuntimeError, ex.displayText().c_str());
        return nullptr;
    }
}

static Pothos::Proxy Proxy_callProxyHelper(ProxyObject *self, const std::string &name, PyObject *args, const int offset = 0)
{
    Pothos::ProxyVector proxyArgs;
    if (args) for (int i = offset; i < PyTuple_Size(args); i++)
    {
        proxyArgs.push_back(PyObjectToProxy(PyTuple_GetItem(args, i)));
    }

    PyThreadStateLock lock; //proxy call could be potentially blocking
    return self->proxy->getHandle()->call(name, proxyArgs.data(), proxyArgs.size());
}

static Pothos::Proxy Proxy_callProxyHelper(ProxyObject *self, PyObject *args, const int offset = 0)
{
    //check the input
    if (not args or PyTuple_Size(args) < 1)
    {
        throw Pothos::Exception("expects at least one arg for call name");
    }

    //extract name and call helper with offset
    const auto name = PyObjectToProxy(PyTuple_GetItem(args, 0)).convert<std::string>();
    return Proxy_callProxyHelper(self, name, args, offset+1);
}

static PyObject *Proxy_callProxy(ProxyObject *self, PyObject *args)
{
    try
    {
        auto proxy = Proxy_callProxyHelper(self, args);
        return makeProxyObject(proxy);
    }
    catch (const Pothos::Exception &ex)
    {
        PyErr_SetString(PyExc_RuntimeError, ex.displayText().c_str());
        return nullptr;
    }
}

static PyObject *Proxy_call(ProxyObject *self, PyObject *args)
{
    try
    {
        auto proxy = Proxy_callProxyHelper(self, args);
        return ProxyToPyObject(proxyEnvTranslate(proxy, getPythonProxyEnv()));
    }
    catch (const Pothos::Exception &ex)
    {
        PyErr_SetString(PyExc_RuntimeError, ex.displayText().c_str());
        return nullptr;
    }
}

static PyObject *Proxy_callFunc(ProxyObject *self, PyObject *args, PyObject *)
{
    try
    {
        auto proxy = Proxy_callProxyHelper(self, "()", args);
        return ProxyToPyObject(proxyEnvTranslate(proxy, getPythonProxyEnv()));
    }
    catch (const Pothos::Exception &ex)
    {
        PyErr_SetString(PyExc_RuntimeError, ex.displayText().c_str());
        return nullptr;
    }
}

static int Proxy_bool(ProxyObject *self)
{
    return bool(*self->proxy)? 1 : 0;
}

static PyObject *Proxy_getEnvironment(ProxyObject *self)
{
    return makeProxyEnvironmentObject(self->proxy->getEnvironment());
}

static PyObject *Proxy_getClassName(ProxyObject *self)
{
    const auto name = self->proxy->getClassName();
    auto proxy = getPythonProxyEnv()->makeProxy(name);
    return ProxyToPyObject(proxy);
}

static PyMethodDef Proxy_methods[] = {
    {"convert", (PyCFunction)Proxy_convert, METH_NOARGS, "Pothos::Proxy::convert()"},
    {"callProxy", (PyCFunction)Proxy_callProxy, METH_VARARGS, "Pothos::Proxy::callProxy(name, args...)"},
    {"call", (PyCFunction)Proxy_call, METH_VARARGS, "Pothos::Proxy::call(name, args...)"},
    {"getEnvironment", (PyCFunction)Proxy_getEnvironment, METH_NOARGS, "Pothos::Proxy::getEnvironment()"},
    {"getClassName", (PyCFunction)Proxy_getClassName, METH_NOARGS, "Pothos::Proxy::getClassName()"},
    {nullptr}  /* Sentinel */
};

long Proxy_Hash(ProxyObject *self)
{
    try
    {
        return long(self->proxy->hashCode());
    }
    catch (...)
    {
        return -1;
    }
}

PyObject* Proxy_Compare(PyObject *o1, PyObject *o2, int opid)
{
    try
    {
        const int cmp = PyObjectToProxy(o1).compareTo(PyObjectToProxy(o2));
        return richCompareFromSimple(cmp, opid);
    }
    catch (const Pothos::Exception &ex)
    {
        PyErr_SetString(PyExc_RuntimeError, ex.displayText().c_str());
        return nullptr;
    }
}

static PyObject *Proxy_toString(ProxyObject *self)
{
    const auto s = self->proxy->toString();
    auto proxy = getPythonProxyEnv()->makeProxy(s);
    return ProxyToPyObject(proxy);
}

PyObject *makeProxyObject(const Pothos::Proxy &proxy)
{
    PyObject *o = PyObject_CallObject((PyObject *)&ProxyType, nullptr);
    auto proxyObject = reinterpret_cast<ProxyObject *>(o);
    *(proxyObject->proxy) = proxy;
    return o;
}

bool isProxyObject(PyObject *obj)
{
    if (obj == nullptr) return false;
    return Py_TYPE(obj) == &ProxyType;
}

void registerProxyType(PyObject *m)
{
    ProxyType.tp_new = PyType_GenericNew;
    ProxyType.tp_name = "PothosProxy";
    ProxyType.tp_basicsize = sizeof(ProxyObject);
    ProxyType.tp_dealloc = (destructor)Proxy_dealloc;
    ProxyType.tp_richcompare = (richcmpfunc)Proxy_Compare;
    ProxyType.tp_hash = (hashfunc)Proxy_Hash;
    ProxyType.tp_str = (reprfunc)Proxy_toString;
    ProxyType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    ProxyType.tp_doc = "Pothos Proxy binding";
    ProxyType.tp_methods = Proxy_methods;
    ProxyType.tp_init = (initproc)Proxy_init;
    ProxyType.tp_getattro = (getattrofunc)Proxy_getattr;
    ProxyType.tp_setattro = (setattrofunc)Proxy_setattr;
    ProxyType.tp_call = (ternaryfunc)Proxy_callFunc;

    ProxyType.tp_as_number = &ProxyNumberMethods;
    #if PY_MAJOR_VERSION >= 3
    ProxyNumberMethods.nb_bool = (inquiry)Proxy_bool;
    #else
    ProxyNumberMethods.nb_nonzero = (inquiry)Proxy_bool;
    #endif

    if (PyType_Ready(&ProxyType) < 0) return;

    Py_INCREF(&ProxyType);
    PyModule_AddObject(m, "Proxy", (PyObject *)&ProxyType);
}
