/***************************************************************************
* Copyright (c) 2016, Johan Mabille and Sylvain Corlay                     *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef PY_TENSOR_HPP
#define PY_TENSOr_HPP

#include <cstddef>
#include <array>
#include <algorithm>
#include "xtensor/xsemantic.hpp"
#include "xtensor/xiterator.hpp"

#include "pycontainer.hpp"
#include "pybuffer_adaptor.hpp"

namespace xt
{
    template <class T, std::size_t N>
    class pytensor;
}

namespace pybind11
{
    namespace detail
    {
        template <class T, std::size_t N>
        struct handle_type_name<xt::pytensor<T, N>>
        {
            static PYBIND11_DESCR name()
            {
                return _("numpy.ndarray[") + make_caster<T>::name() + _("]");
            }
        };

        template <class T, std::size_t N>
        struct pyobject_caster<xt::pytensor<T, N>>
        {
            using type = xt::pytensor<T, N>;

            bool load(handle src, bool)
            {
                value = type::ensure(src);
                return static_cast<bool>(value);
            }

            static handle cast(const handle& src, return_value_policy, handle)
            {
                return src.inc_ref();
            }

            PYBIND11_TYPE_CASTER(type, handle_type_name<type>::name());
        };
    }
}

namespace xt
{
    template <class T, std::size_t N>
    struct xcontainer_inner_types<pytensor<T, N>>
    {
        using container_type = pybuffer_adaptor<T>;
        using shape_type = std::array<typename container_type::size_type, N>;
        using strides_type = shape_type;
        using backstrides_type = shape_type;
        using temporary_type = pytensor<T, N>;
    };

    template <class T, std::size_t N>
    class pytensor : public pycontainer<pytensor<T, N>>,
                     public xcontainer_semantic<pytensor<T, N>>
    {

    public:

        using self_type = pytensor<T, N>;
        using semantic_base = xcontainer_semantic<self_type>;
        using base_type = pycontainer<pytensor<T, N>>;
        using container_type = typename base_type::container_type;
        using pointer = typename base_type::pointer;
        using size_type = typename base_type::size_type;
        using shape_type = typename base_type::shape_type;
        using strides_type = typename base_type::strides_type;
        using backstrides_type = typename base_type::backstrides_type;

        pytensor() = default;

        pytensor(pybind11::handle h, borrowed_t);
        pytensor(pybind11::handle h, stolen_t);
        pytensor(const pybind11::object &o);
        
        pytensor(const shape_type& shape, const strides_type& strides);
        explicit pytensor(const shape_type& shape);

        template <class E>
        pytensor(const xexpression<E>& e);

        template <class E>
        self_type& operator=(const xexpression<E>& e);

        void reshape(const shape_type& shape);
        void reshape(const shape_type& shape, const strides_type& strides);

        using base_type::begin;
        using base_type::end;

        static self_type ensure(pybind11::handle h);
        static bool check_(pybind11::handle h);

    private:

        shape_type m_shape;
        strides_type m_strides;
        strides_type m_backstrides;
        container_type m_data;

        void init_tensor(const shape_type& shape, const strides_type& strides);
        void init_from_python();
        void adapt_strides();

        const shape_type& shape_impl() const;
        const strides_type& strides_impl() const;
        const backstrides_type& backstrides_impl() const;

        container_type& data_impl();
        const container_type& data_impl() const;

        friend class pycontainer<pytensor<T, N>>;
    };

    /***************************
     * pytensor implementation *
     ***************************/

    template <class T, std::size_t N>
    inline pytensor<T, N>::pytensor(pybind11::handle h, borrowed_t)
        : base_type(h, borrowed)
    {
        init_from_python();
    }

    template <class T, std::size_t N>
    inline pytensor<T, N>::pytensor(pybind11::handle h, stolen_t)
        : base_type(h, stolen)
    {
        init_from_python();
    }

    template <class T, std::size_t N>
    inline pytensor<T, N>::pytensor(const pybind11::object& o)
        : base_type(base_type::raw_array_t(o.ptr()), stolen)
    {
        if(!this->m_ptr)
            throw pybind11::error_already_set();
        init_from_python();
    }

    template <class T, std::size_t N>
    inline pytensor<T, N>::pytensor(const shape_type& shape,
                                    const strides_type& strides)
    {
        init_tensor(shape, strides);
    }

    template <class T, std::size_t N>
    inline pytensor<T, N>::pytensor(const shape_type& shape)
    {
        base_type::fill_default_strides(shape, m_strides);
        init_tensor(shape, m_strides);
    }

    template <class T, std::size_t N>
    template <class E>
    inline pytensor<T, N>::pytensor(const xexpression<E>& e)
    {
        semantic_base::assign(e);
    }

    template <class T, std::size_t N>
    template <class E>
    inline auto pytensor<T, N>::operator=(const xexpression<E>& e) -> self_type&
    {
        return semantic_base::operator=(e);
    }

    template <class T, std::size_t N>
    inline void pytensor<T, N>::reshape(const shape_type& shape)
    {
        if(shape != m_shape)
        {
            strides_type strides;
            base_type::fill_default_strides(shape, strides);
            reshape(shape, strides);
        }
    }

    template <class T, std::size_t N>
    inline void pytensor<T, N>::reshape(const shape_type& shape, const strides_type& strides)
    {
        self_type tmp(shape, strides);
        *this = std::move(tmp);
    }

    template <class T, std::size_t N>
    inline auto pytensor<T, N>::ensure(pybind11::handle h) -> self_type
    {
        auto result = pybind11::reinterpret_steal<self_type>(base_type::raw_array_t(h.ptr()));
        if(result.ptr() == nullptr)
            PyErr_Clear();
        return result;
    }

    template <class T, std::size_t N>
    inline bool pytensor<T, N>::check_(pybind11::handle h)
    {
        int type_num = detail::numpy_traits<T>::type_num;
        return PyArray_Check(h.ptr()) && PyArray_EquivTypenums(PyArray_TYPE(h.ptr()), type_num);
    }
    
    template <class T, std::size_t N>
    inline void pytensor<T, N>::init_tensor(const shape_type& shape, const strides_type& strides)
    {
        npy_intp python_strides[N];
        std::transform(strides.beign(), strides.end(), python_strides,
                [](auto v) { return sizeof(T) * v; });
        int flags = NPY_ARRAY_ALIGNED;
        if(!std::is_const<T>::value)
        {
            flags |= NPY_ARRAY_WRITEABLE;
        }
        int type_num = detail::numpy_traits<T>::type_num;
        
        auto tmp = pybind11::reinterpret_steal<pybind11::object>(
                PyArray_New(&PyArray_Type, N, reinterpret_cast<npy_intp*>(shape.data()),
                            type_num, python_strides, nullptr, sizeof(T), flags, nullptr)
                );
        
        if(!tmp)
            throw std::runtime_error("NumPy: unable to create ndarray");

        this->m_ptr = tmp.release().ptr();
        m_shape = shape;
        m_strides = strides;
        adapt_strides();
    }

    template <class T, std::size_t N>
    inline void pytensor<T, N>::init_from_python()
    {
        if(PyArray_NDIM(this->m_ptr) != N)
            throw std::runtime_error("NumPy: ndarray has incorrect number of dimensions");

        std::copy(PyArray_DIMS(this->m_ptr), PyArray_DIMS(this->m_ptr) + N, m_shape.begin());
        std::transform(PyArray_STRIDES(this->m_ptr), PyArray_STRIDES(this->m_ptr) + N, m_strides.begin(),
                [](auto v) { return v / sizeof(T); });
        adapt_strides();
        m_data = container_type(reinterpret_cast<pointer>(PyArray_DATA(this->m_ptr)),
                                static_cast<size_type>(PyArray_SIZE(this->m_ptr)));
    }

    template <class T, std::size_t N>
    inline void pytensor<T, N>::adapt_strides()
    {
        for(size_type i = 0; i < m_shape.size(); ++i)
        {
            if(m_shape[i] == 1)
            {
                m_strides[i] = 0;
                m_backstrides[i] = 0;
            }
            else
            {
                m_backstrides[i] = m_strides[i] * (m_shape[i] - 1);
            }
        }
    }

    template <class T, std::size_t N>
    inline auto pytensor<T, N>::shape_impl() const -> const shape_type&
    {
        return m_shape;
    }

    template <class T, std::size_t N>
    inline auto pytensor<T, N>::strides_impl() const -> const strides_type&
    {
        return m_strides;
    }

    template <class T, std::size_t N>
    inline auto pytensor<T, N>::backstrides_impl() const -> const backstrides_type&
    {
        return m_backstrides;
    }

    template <class T, std::size_t N>
    inline auto pytensor<T, N>::data_impl() -> container_type&
    {
        return m_data;
    }

    template <class T, std::size_t N>
    inline auto pytensor<T, N>::data_impl() const -> const container_type&
    {
        return m_data;
    }

}

#endif

