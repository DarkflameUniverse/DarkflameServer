// ----------------------------------------------------------------------------------------------
// Copyright (c) Mårten Rånge.
// ----------------------------------------------------------------------------------------------
// This source code is subject to terms and conditions of the Microsoft Public License. A
// copy of the license can be found in the License.html file at the root of this distribution.
// If you cannot locate the  Microsoft Public License, please send an email to
// dlr@microsoft.com. By using this source code in any fashion, you are agreeing to be bound
//  by the terms of the Microsoft Public License.
// ----------------------------------------------------------------------------------------------
// You must not remove this notice, or any other, from this software.
// ----------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------
#ifndef CPPLINQ__HEADER_GUARD
#   define CPPLINQ__HEADER_GUARD

#undef min
#undef max
#define NOMINMAX

// ----------------------------------------------------------------------------
#include <algorithm>
#include <cassert>
#include <climits>
#include <exception>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <type_traits>
#include <vector>
#include <limits>
#// ----------------------------------------------------------------------------
#ifdef _MSC_VER
#   pragma warning (push)
#       pragma warning (disable:4512)   // 'class' : assignment operator could not be generated
                                        // Rationale: cpplinq doesn't rely on assignment operators
#       pragma warning (disable:4702)   // unreachable code
                                        // Rationale: Depending on the predicates and the calling
                                        // code the compiler might detect unreacable code that with
                                        // other predicates are reachable.

#		pragma warning (disable:4003)
#endif

#define NOMINMAX // so we wont have conflicts with std:: thing
// ----------------------------------------------------------------------------
#define CPPLINQ_METHOD
#define CPPLINQ_INLINEMETHOD inline
#define CPPLINQ_NOEXCEPT throw ()
// ----------------------------------------------------------------------------

// TODO:    Struggled with getting slice protection
//          and assignment operator detection for MINGW
//          Look into using delete standard functions
// #define CPPLINQ_DETECT_INVALID_METHODS

// ----------------------------------------------------------------------------
namespace cpplinq
{
    // -------------------------------------------------------------------------

    typedef std::size_t size_type;

    // -------------------------------------------------------------------------

    struct base_exception : std::exception
    {
        virtual const char* what ()  const CPPLINQ_NOEXCEPT
        {
            return "base_exception";
        }
    };

    struct programming_error_exception : base_exception
    {
        virtual const char* what ()  const CPPLINQ_NOEXCEPT
        {
            return "programming_error_exception";
        }
    };

    struct sequence_empty_exception : base_exception
    {
        virtual const char* what ()  const CPPLINQ_NOEXCEPT
        {
            return "sequence_empty_exception";
        }
    };

    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // Tedious implementation details of cpplinq
    // -------------------------------------------------------------------------

    namespace detail
    {

        // -------------------------------------------------------------------------

        size_type const invalid_size = static_cast<size_type>(-1);

        // -------------------------------------------------------------------------

        template<typename TValue>
        struct cleanup_type
        {
            typedef typename std::remove_const<
                typename std::remove_reference<TValue>::type
                >::type                                         type;
        };

        template<typename TRangeBuilder, typename TRange>
        struct get_builtup_type
        {
            static TRangeBuilder get_builder ();
            static TRange get_range ();

            typedef decltype (get_builder ().build (get_range ()))  type;
        };

        template<typename TPredicate, typename TValue>
        struct get_transformed_type
        {
            static TValue                       get_value ();
            static TPredicate                   get_predicate ();

            typedef     decltype (get_predicate ()(get_value ()))   raw_type    ;
            typedef     typename cleanup_type<raw_type>::type       type        ;
        };

        template<typename TArray>
        struct get_array_properties;

        template<typename TValue, int Size>
        struct get_array_properties<TValue[Size]>
        {
            enum
            {
                size = Size ,
            };

            typedef typename    cleanup_type<TValue>::type  value_type      ;
            typedef             value_type*                 iterator_type   ;
        };

        template<typename TValue>
        struct opt
        {
            typedef     TValue  value_type;

            CPPLINQ_INLINEMETHOD opt () CPPLINQ_NOEXCEPT
                :   is_initialized (false)
            {
            }

            CPPLINQ_INLINEMETHOD explicit opt (value_type && value)
                :   is_initialized      (true)
            {
                new (&storage) value_type (std::move (value));
            }

            CPPLINQ_INLINEMETHOD explicit opt (value_type const & value)
                :   is_initialized      (true)
            {
                new (&storage) value_type (value);
            }

            CPPLINQ_INLINEMETHOD ~opt () CPPLINQ_NOEXCEPT
            {
                auto ptr = get_ptr ();
                if (ptr)
                {
                    ptr->~value_type ();
                }
                is_initialized = false;
            }

            CPPLINQ_INLINEMETHOD opt (opt const & v)
                :   is_initialized      (v.is_initialized)
            {
                if (v.is_initialized)
                {
                    copy (&storage  , &v.storage    );
                }
            }

            CPPLINQ_INLINEMETHOD opt (opt && v)  CPPLINQ_NOEXCEPT
                :   is_initialized      (v.is_initialized)
            {
                if (v.is_initialized)
                {
                    move (&storage  , &v.storage    );
                }
                v.is_initialized = false;
            }

            CPPLINQ_METHOD void swap (opt & v)
            {
                if (is_initialized && v.is_initialized)
                {
                    storage_type tmp;

                    move (&tmp          , &storage      );
                    move (&storage      , &v.storage    );
                    move (&v.storage    , &tmp          );
                }
                else if (is_initialized)
                {
                    move (&v.storage    , &storage      );
                    v.is_initialized= true;
                    is_initialized  = false;
                }
                else if (v.is_initialized)
                {
                    move (&storage      , &v.storage    );
                    v.is_initialized= false;
                    is_initialized  = true;
                }
                else
                {
                    // Do nothing
                }
            }

            CPPLINQ_INLINEMETHOD opt & operator= (opt const & v)
            {
                if (this == std::addressof (v))
                {
                    return *this;
                }

                opt<value_type> o (v);

                swap (o);

                return *this;
            }

            CPPLINQ_INLINEMETHOD opt & operator= (opt && v)
            {
                if (this == std::addressof (v))
                {
                    return *this;
                }

                swap (v);

                return *this;
            }

            CPPLINQ_INLINEMETHOD opt & operator= (value_type v)
            {
                return *this = opt (std::move (v));
            }

            CPPLINQ_INLINEMETHOD void clear () CPPLINQ_NOEXCEPT
            {
                opt empty;
                swap (empty);
            }

            CPPLINQ_INLINEMETHOD value_type const * get_ptr () const CPPLINQ_NOEXCEPT
            {
                if (is_initialized)
                {
                    return reinterpret_cast<value_type const *> (&storage);
                }
                else
                {
                    return nullptr;
                }
            }

            CPPLINQ_INLINEMETHOD value_type * get_ptr () CPPLINQ_NOEXCEPT
            {
                if (is_initialized)
                {
                    return reinterpret_cast<value_type*> (&storage);
                }
                else
                {
                    return nullptr;
                }
            }

            CPPLINQ_INLINEMETHOD value_type const & get () const CPPLINQ_NOEXCEPT
            {
                assert (is_initialized);
                return *get_ptr ();
            }

            CPPLINQ_INLINEMETHOD value_type & get () CPPLINQ_NOEXCEPT
            {
                assert (is_initialized);
                return *get_ptr ();
            }

            CPPLINQ_INLINEMETHOD bool has_value () const CPPLINQ_NOEXCEPT
            {
                return is_initialized;
            }

            typedef bool (opt::*type_safe_bool_type) () const;

            CPPLINQ_INLINEMETHOD operator type_safe_bool_type () const CPPLINQ_NOEXCEPT
            {
                return is_initialized ? &opt::has_value : nullptr;
            }

            CPPLINQ_INLINEMETHOD value_type const & operator* () const CPPLINQ_NOEXCEPT
            {
                return get ();
            }

            CPPLINQ_INLINEMETHOD value_type & operator* () CPPLINQ_NOEXCEPT
            {
                return get ();
            }

            CPPLINQ_INLINEMETHOD value_type const * operator-> () const CPPLINQ_NOEXCEPT
            {
                return get_ptr ();
            }

            CPPLINQ_INLINEMETHOD value_type * operator-> () CPPLINQ_NOEXCEPT
            {
                return get_ptr ();
            }

        private:
            typedef typename std::aligned_storage<
                    sizeof (value_type)
                ,   std::alignment_of<value_type>::value
                >::type storage_type    ;

            storage_type    storage         ;
            bool            is_initialized  ;

            CPPLINQ_INLINEMETHOD static void move (
                    storage_type * to
                ,   storage_type * from
                ) CPPLINQ_NOEXCEPT
            {
                auto f = reinterpret_cast<value_type*> (from);
                new (to) value_type (std::move (*f));
                f->~value_type ();
            }

            CPPLINQ_INLINEMETHOD static void copy (
                    storage_type * to
                ,   storage_type const * from
                )
            {
                auto f = reinterpret_cast<value_type const *> (from);
                new (to) value_type (*f);
            }


        };

        // -------------------------------------------------------------------------
        // The generic interface
        // -------------------------------------------------------------------------
        // _range classes:
        //      inherit base_range
        //      COPYABLE
        //      MOVEABLE (movesemantics)
        //      typedef                 ...         this_type       ;
        //      typedef                 ...         value_type      ;
        //      typedef                 ...         return_type     ;   // value_type | value_type const &
        //      enum { returns_reference = 0|1 };
        //      return_type front () const
        //      bool next ()
        //      template<typename TRangeBuilder>
        //      typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
        // -------------------------------------------------------------------------
        // _builder classes:
        //      inherit base_builder
        //      COPYABLE
        //      MOVEABLE (movesemantics)
        //      typedef                 ...         this_type       ;
        //      template<typename TRange>
        //      TAggregated build (TRange range) const
        // -------------------------------------------------------------------------

        struct base_range
        {
#ifdef CPPLINQ_DETECT_INVALID_METHODS
        protected:
            // In order to prevent object slicing

            CPPLINQ_INLINEMETHOD base_range () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD base_range (base_range const &) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD base_range (base_range &&) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD ~base_range () CPPLINQ_NOEXCEPT
            {
            }

        private:
            CPPLINQ_INLINEMETHOD base_range & operator= (base_range const &);
            CPPLINQ_INLINEMETHOD base_range & operator= (base_range &&);
#endif
        };

        struct base_builder
        {
#ifdef CPPLINQ_DETECT_INVALID_METHODS
        protected:
            // In order to prevent object slicing

            CPPLINQ_INLINEMETHOD base_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD base_builder (base_builder const &) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD base_builder (base_builder &&) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD ~base_builder () CPPLINQ_NOEXCEPT
            {
            }

        private:
            CPPLINQ_INLINEMETHOD base_builder & operator= (base_builder const &);
            CPPLINQ_INLINEMETHOD base_builder & operator= (base_builder &&);
#endif
        };

        template<typename TValueIterator>
        struct from_range : base_range
        {
            static TValueIterator get_iterator ();

            typedef                 from_range<TValueIterator>          this_type       ;
            typedef                 TValueIterator                      iterator_type   ;

            typedef                 decltype (*get_iterator ())         raw_value_type  ;
            typedef        typename cleanup_type<raw_value_type>::type  value_type      ;
            typedef                 value_type const &                  return_type     ;
            enum
            {
                returns_reference = 1,
            };

            iterator_type           current ;
            iterator_type           upcoming;
            iterator_type           end     ;


            CPPLINQ_INLINEMETHOD from_range (
                    iterator_type begin
                ,   iterator_type end
                ) CPPLINQ_NOEXCEPT
                :   current (std::move (begin))
                ,   upcoming(current)
                ,   end     (std::move (end))
            {
            }

            CPPLINQ_INLINEMETHOD from_range (from_range const & v) CPPLINQ_NOEXCEPT
                :   current (v.current)
                ,   upcoming(v.upcoming)
                ,   end     (v.end)
            {
            }

            CPPLINQ_INLINEMETHOD from_range (from_range && v) CPPLINQ_NOEXCEPT
                :   current (std::move (v.current))
                ,   upcoming(std::move (v.upcoming))
                ,   end     (std::move (v.end))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                assert (current != upcoming);
                assert (current != end);

                return *current;
            }

            CPPLINQ_INLINEMETHOD bool next () CPPLINQ_NOEXCEPT
            {
                if (upcoming == end)
                {
                    return false;
                }

                current = upcoming;
                ++upcoming;
                return true;
            }
        };

        // -------------------------------------------------------------------------

        // -------------------------------------------------------------------------

        template<typename TContainer>
        struct from_copy_range : base_range
        {
            typedef                 from_copy_range<TContainer>         this_type       ;

            typedef                 TContainer                          container_type  ;
            typedef        typename TContainer::const_iterator          iterator_type   ;
            typedef        typename TContainer::value_type              value_type      ;
            typedef                 value_type const &                  return_type     ;
            enum
            {
                returns_reference = 1 ,
            };

            container_type          container   ;

            iterator_type           current     ;
            iterator_type           upcoming    ;
            iterator_type           end         ;

            CPPLINQ_INLINEMETHOD from_copy_range (
                    container_type&&        container
                )
                :   container   (std::move (container))
                ,   current     (container.begin ())
                ,   upcoming    (container.begin ())
                ,   end         (container.end ())
            {
            }

            CPPLINQ_INLINEMETHOD from_copy_range (
                    container_type const &  container
                )
                :   container   (container)
                ,   current     (container.begin ())
                ,   upcoming    (container.begin ())
                ,   end         (container.end ())
            {
            }

            CPPLINQ_INLINEMETHOD from_copy_range (from_copy_range const & v)
                :   container   (v.container)
                ,   current     (v.current)
                ,   upcoming    (v.upcoming)
                ,   end         (v.end)
            {
            }

            CPPLINQ_INLINEMETHOD from_copy_range (from_copy_range && v) CPPLINQ_NOEXCEPT
                :   container   (std::move (v.container))
                ,   current     (std::move (v.current))
                ,   upcoming    (std::move (v.upcoming))
                ,   end         (std::move (v.end))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                assert (current != upcoming);
                assert (current != end);

                return *current;
            }

            CPPLINQ_INLINEMETHOD bool next () CPPLINQ_NOEXCEPT
            {
                if (upcoming == end)
                {
                    return false;
                }

                current = upcoming;
                ++upcoming;
                return true;
            }
        };

        // -------------------------------------------------------------------------

        struct int_range : base_range
        {
            typedef                 int_range                           this_type       ;
            typedef                 int                                 value_type      ;
            typedef                 int                                 return_type     ;
            enum
            {
                returns_reference = 0   ,
            };

            int                     current ;
            int                     end     ;

            static int get_current (int begin, int end)
            {
                return (begin < end ? begin : end) - 1; // -1 in order to start one-step before the first element
            }

            static int get_end (int begin, int end)     // -1 in order to avoid an extra test in next
            {
                return (begin < end ? end : begin) - 1;
            }

            CPPLINQ_INLINEMETHOD int_range (
                    int begin
                ,   int end
                ) CPPLINQ_NOEXCEPT
                :   current (get_current (begin, end))
                ,   end     (get_end (begin,end))
            {
            }

            CPPLINQ_INLINEMETHOD int_range (int_range const & v) CPPLINQ_NOEXCEPT
                :   current (v.current)
                ,   end     (v.end)
            {
            }

            CPPLINQ_INLINEMETHOD int_range (int_range && v) CPPLINQ_NOEXCEPT
                :   current (std::move (v.current))
                ,   end     (std::move (v.end))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return current;
            }

            CPPLINQ_INLINEMETHOD bool next () CPPLINQ_NOEXCEPT
            {
                if (current >= end)
                {
                    return false;
                }

                ++current;

                return true;
            }
        };

        // -------------------------------------------------------------------------

        template <typename TValue>
        struct repeat_range : base_range
        {
            typedef                 repeat_range<TValue>                this_type       ;
            typedef                 TValue                              value_type      ;
            typedef                 TValue                              return_type     ;
            enum
            {
                returns_reference = 0   ,
            };

            TValue                  value       ;
            size_type               remaining   ;

            CPPLINQ_INLINEMETHOD repeat_range (
                    value_type element
                ,   size_type count
                ) CPPLINQ_NOEXCEPT
                :   value       (std::move (element))
                ,   remaining   (count)
            {
            }

            CPPLINQ_INLINEMETHOD repeat_range (repeat_range const & v) CPPLINQ_NOEXCEPT
                :   value       (v.value)
                ,   remaining   (v.remaining)
            {
            }

            CPPLINQ_INLINEMETHOD repeat_range (repeat_range && v) CPPLINQ_NOEXCEPT
                :   value       (std::move (v.value))
                ,   remaining   (std::move (v.remaining))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return value;
            }

            CPPLINQ_INLINEMETHOD bool next () CPPLINQ_NOEXCEPT
            {
                if (remaining == 0U)
                {
                    return false;
                }

                --remaining;

                return true;
            }
        };

        // -------------------------------------------------------------------------

        template <typename TValue>
        struct empty_range : base_range
        {
            typedef                 empty_range<TValue>                 this_type       ;
            typedef                 TValue                              value_type      ;
            typedef                 TValue                              return_type     ;
            enum
            {
                returns_reference = 0   ,
            };

            CPPLINQ_INLINEMETHOD empty_range () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD empty_range (empty_range const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD empty_range (empty_range && v) CPPLINQ_NOEXCEPT
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                assert (false);
                throw programming_error_exception ();
            }

            CPPLINQ_INLINEMETHOD bool next () CPPLINQ_NOEXCEPT
            {
                return false;
            }
        };

        // -------------------------------------------------------------------------

        template <typename TValue>
        struct singleton_range : base_range
        {
            typedef                 singleton_range<TValue>             this_type       ;
            typedef                 TValue                              value_type      ;
            typedef                 TValue const &                      return_type     ;

            enum
            {
                returns_reference = 1   ,
            };

            value_type  value   ;
            bool        done    ;

            CPPLINQ_INLINEMETHOD singleton_range (TValue const & value)
                :   value   (value)
                ,   done    (false)
            {
            }

            CPPLINQ_INLINEMETHOD singleton_range (TValue&& value) CPPLINQ_NOEXCEPT
                :   value   (std::move (value))
                ,   done    (false)
            {
            }

            CPPLINQ_INLINEMETHOD singleton_range (singleton_range const & v) CPPLINQ_NOEXCEPT
                :   value   (v.value)
                ,   done    (v.done)
            {
            }

            CPPLINQ_INLINEMETHOD singleton_range (singleton_range && v) CPPLINQ_NOEXCEPT
                :   value   (std::move (v.value))
                ,   done    (std::move (v.done))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const CPPLINQ_NOEXCEPT
            {
                return value;
            }

            CPPLINQ_INLINEMETHOD bool next () CPPLINQ_NOEXCEPT
            {
                auto d  = done;
                done    = true;
                return !d;
            }
        };

        // -------------------------------------------------------------------------

        struct sorting_range : base_range
        {
#ifdef CPPLINQ_DETECT_INVALID_METHODS
        protected:
            // In order to prevent object slicing

            CPPLINQ_INLINEMETHOD sorting_range () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD sorting_range (sorting_range const &) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD sorting_range (sorting_range &&) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD ~sorting_range () CPPLINQ_NOEXCEPT
            {
            }
        private:
            CPPLINQ_INLINEMETHOD sorting_range & operator= (sorting_range const &);
            CPPLINQ_INLINEMETHOD sorting_range & operator= (sorting_range &&);
#endif
        };

        template<typename TRange, typename TPredicate>
        struct orderby_range : sorting_range
        {

            typedef                 orderby_range<TRange, TPredicate>   this_type               ;
            typedef                 TRange                              range_type              ;
            typedef                 TPredicate                          predicate_type          ;

            typedef                 typename TRange::value_type         value_type              ;
            typedef                 typename TRange::return_type        forwarding_return_type  ;
            typedef                 value_type const &                  return_type             ;
            enum
            {
                forward_returns_reference   = TRange::returns_reference ,
                returns_reference           = 1                         ,
            };

            range_type              range           ;
            predicate_type          predicate       ;
            bool                    sort_ascending  ;

            size_type               current         ;
            std::vector<value_type> sorted_values   ;

            CPPLINQ_INLINEMETHOD orderby_range (
                    range_type      range
                ,   predicate_type  predicate
                ,   bool            sort_ascending
                ) CPPLINQ_NOEXCEPT
                :   range           (std::move (range))
                ,   predicate       (std::move (predicate))
                ,   sort_ascending  (sort_ascending)
                ,   current         (invalid_size)
            {
                static_assert (
                        !std::is_convertible<range_type, sorting_range>::value
                    ,   "orderby may not follow orderby or thenby"
                    );
            }

            CPPLINQ_INLINEMETHOD orderby_range (orderby_range const & v)
                :   range           (v.range)
                ,   predicate       (v.predicate)
                ,   sort_ascending  (v.sort_ascending)
                ,   current         (v.current)
                ,   sorted_values   (v.sorted_values)
            {
            }

            CPPLINQ_INLINEMETHOD orderby_range (orderby_range && v) CPPLINQ_NOEXCEPT
                :   range           (std::move (v.range))
                ,   predicate       (std::move (v.predicate))
                ,   sort_ascending  (std::move (v.sort_ascending))
                ,   current         (std::move (v.current))
                ,   sorted_values   (std::move (v.sorted_values))
            {
            }

            CPPLINQ_INLINEMETHOD forwarding_return_type forwarding_front () const
            {
                return range.front ();
            }

            CPPLINQ_INLINEMETHOD bool forwarding_next ()
            {
                return range.next ();
            }

            CPPLINQ_INLINEMETHOD bool compare_values (value_type const & l, value_type const & r) const
            {
                if (sort_ascending)
                {
                    return predicate (l) < predicate (r);
                }
                else
                {
                    return predicate (r) < predicate (l);
                }
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return sorted_values[current];
            }

            CPPLINQ_METHOD bool next ()
            {
                if (current == invalid_size)
                {
                    sorted_values.clear ();

                    while (range.next ())
                    {
                        sorted_values.push_back (range.front ());
                    }

                    if (sorted_values.size () == 0)
                    {
                        return false;
                    }

                    std::sort (
                            sorted_values.begin ()
                        ,   sorted_values.end ()
                        ,   [this] (value_type const & l, value_type const & r)
                            {
                                return this->compare_values (l,r);
                            }
                        );

                    current = 0U;
                    return true;
                }

                if (current < sorted_values.size ())
                {
                    ++current;
                }

                return current < sorted_values.size ();
            }
        };

        template<typename TPredicate>
        struct orderby_builder : base_builder
        {
            typedef                 orderby_builder<TPredicate> this_type       ;
            typedef                 TPredicate                  predicate_type  ;

            predicate_type          predicate       ;
            bool                    sort_ascending  ;

            CPPLINQ_INLINEMETHOD explicit orderby_builder (predicate_type predicate, bool sort_ascending) CPPLINQ_NOEXCEPT
                :   predicate       (std::move (predicate))
                ,   sort_ascending  (sort_ascending)
            {
            }

            CPPLINQ_INLINEMETHOD orderby_builder (orderby_builder const & v)
                :   predicate       (v.predicate)
                ,   sort_ascending  (v.sort_ascending)
            {
            }

            CPPLINQ_INLINEMETHOD orderby_builder (orderby_builder && v) CPPLINQ_NOEXCEPT
                :   predicate       (std::move (v.predicate))
                ,   sort_ascending  (std::move (v.sort_ascending))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD orderby_range<TRange, TPredicate> build (TRange range) const
            {
                return orderby_range<TRange, TPredicate>(std::move (range), predicate, sort_ascending);
            }

        };

        // -------------------------------------------------------------------------

        template<typename TRange, typename TPredicate>
        struct thenby_range : sorting_range
        {
            typedef                 thenby_range<TRange, TPredicate>        this_type               ;
            typedef                 TRange                                  range_type              ;
            typedef                 TPredicate                              predicate_type          ;

            typedef                 typename TRange::value_type             value_type              ;
            typedef                 typename TRange::forwarding_return_type forwarding_return_type  ;
            typedef                 value_type const &                      return_type             ;
            enum
            {
                forward_returns_reference   = TRange::forward_returns_reference ,
                returns_reference           = 1                                 ,
            };

            range_type              range           ;
            predicate_type          predicate       ;
            bool                    sort_ascending  ;

            size_type               current         ;
            std::vector<value_type> sorted_values   ;

            CPPLINQ_INLINEMETHOD thenby_range (
                    range_type      range
                ,   predicate_type  predicate
                ,   bool            sort_ascending
                ) CPPLINQ_NOEXCEPT
                :   range           (std::move (range))
                ,   predicate       (std::move (predicate))
                ,   sort_ascending  (sort_ascending)
                ,   current         (invalid_size)
            {
                static_assert (
                        std::is_convertible<range_type, sorting_range>::value
                    ,   "thenby may only follow orderby or thenby"
                    );
            }

            CPPLINQ_INLINEMETHOD thenby_range (thenby_range const & v)
                :   range           (v.range)
                ,   predicate       (v.predicate)
                ,   sort_ascending  (v.sort_ascending)
                ,   current         (v.current)
                ,   sorted_values   (v.sorted_values)
            {
            }

            CPPLINQ_INLINEMETHOD thenby_range (thenby_range && v) CPPLINQ_NOEXCEPT
                :   range           (std::move (v.range))
                ,   predicate       (std::move (v.predicate))
                ,   sort_ascending  (std::move (v.sort_ascending))
                ,   current         (std::move (v.current))
                ,   sorted_values   (std::move (v.sorted_values))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD forwarding_return_type forwarding_front () const
            {
                return range.front ();
            }

            CPPLINQ_INLINEMETHOD bool forwarding_next ()
            {
                return range.next ();
            }

            CPPLINQ_INLINEMETHOD bool compare_values (value_type const & l, value_type const & r) const
            {
                auto pless = range.compare_values (l,r);
                if (pless)
                {
                    return true;
                }

                auto pgreater = range.compare_values (r,l);
                if (pgreater)
                {
                    return false;
                }

                if (sort_ascending)
                {
                    return predicate (l) < predicate (r);
                }
                else
                {
                    return predicate (r) < predicate (l);
                }
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return sorted_values[current];
            }

            CPPLINQ_METHOD bool next ()
            {
                if (current == invalid_size)
                {
                    sorted_values.clear ();

                    while (range.forwarding_next ())
                    {
                        sorted_values.push_back (range.forwarding_front ());
                    }

                    if (sorted_values.size () == 0)
                    {
                        return false;
                    }

                    std::sort (
                            sorted_values.begin ()
                        ,   sorted_values.end ()
                        ,   [this] (value_type const & l, value_type const & r)
                            {
                                return this->compare_values (l,r);
                            }
                        );

                    current = 0U;
                    return true;
                }

                if (current < sorted_values.size ())
                {
                    ++current;
                }

                return current < sorted_values.size ();
            }
        };

        template<typename TPredicate>
        struct thenby_builder : base_builder
        {
            typedef                 thenby_builder<TPredicate>  this_type       ;
            typedef                 TPredicate                  predicate_type  ;

            predicate_type          predicate       ;
            bool                    sort_ascending  ;

            CPPLINQ_INLINEMETHOD explicit thenby_builder (predicate_type predicate, bool sort_ascending) CPPLINQ_NOEXCEPT
                :   predicate       (std::move (predicate))
                ,   sort_ascending  (sort_ascending)
            {
            }

            CPPLINQ_INLINEMETHOD thenby_builder (thenby_builder const & v)
                :   predicate       (v.predicate)
                ,   sort_ascending  (v.sort_ascending)
            {
            }

            CPPLINQ_INLINEMETHOD thenby_builder (thenby_builder && v) CPPLINQ_NOEXCEPT
                :   predicate       (std::move (v.predicate))
                ,   sort_ascending  (std::move (v.sort_ascending))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD thenby_range<TRange, TPredicate> build (TRange range) const
            {
                return thenby_range<TRange, TPredicate>(std::move (range), predicate, sort_ascending);
            }

        };

        // -------------------------------------------------------------------------

        template<typename TRange>
        struct reverse_range : base_range
        {
            typedef                 reverse_range<TRange>                       this_type           ;
            typedef                 TRange                                      range_type          ;

            typedef                 typename TRange::value_type                 value_type          ;
            typedef                 value_type const &                          return_type         ;

            typedef                 std::vector<value_type>                     stack_type          ;

            enum
            {
                returns_reference   = 1     ,
            };


            range_type                  range               ;
            size_type                   capacity            ;
            std::vector<value_type>     reversed            ;
            bool                        start               ;

            CPPLINQ_INLINEMETHOD reverse_range (
                    range_type          range
                ,   size_type           capacity
                ) CPPLINQ_NOEXCEPT
                :   range               (std::move (range))
                ,   capacity            (capacity)
                ,   start               (true)
            {
            }

            CPPLINQ_INLINEMETHOD reverse_range (reverse_range const & v) CPPLINQ_NOEXCEPT
                :   range               (v.range)
                ,   capacity            (v.capacity)
                ,   reversed            (v.reversed)
                ,   start               (v.start)
            {
            }

            CPPLINQ_INLINEMETHOD reverse_range (reverse_range && v) CPPLINQ_NOEXCEPT
                :   range               (std::move (v.range))
                ,   capacity            (std::move (v.capacity))
                ,   reversed            (std::move (v.reversed))
                ,   start               (std::move (v.start))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const CPPLINQ_NOEXCEPT
            {
                assert (!start);
                assert (!reversed.empty ());
                return reversed[reversed.size () - 1];
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                if (start)
                {
                    start = false;

                    reversed.clear ();
                    reversed.reserve (capacity);

                    while (range.next ())
                    {
                        reversed.push_back (range.front ());
                    }

                    return !reversed.empty ();
                }

                if (reversed.empty ())
                {
                    return false;
                }

                reversed.pop_back ();

                return !reversed.empty ();
            }
        };

        struct reverse_builder : base_builder
        {
            typedef             reverse_builder     this_type   ;

            size_type           capacity                        ;

            CPPLINQ_INLINEMETHOD reverse_builder (size_type capacity) CPPLINQ_NOEXCEPT
                :   capacity (capacity)
            {
            }

            CPPLINQ_INLINEMETHOD reverse_builder (reverse_builder const & v) CPPLINQ_NOEXCEPT
                :   capacity (v.capacity)
            {
            }

            CPPLINQ_INLINEMETHOD reverse_builder (reverse_builder && v) CPPLINQ_NOEXCEPT
                :   capacity (std::move (v.capacity))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD reverse_range<TRange> build (TRange range) const
            {
                return reverse_range<TRange> (std::move (range), capacity);
            }
        };

        // -------------------------------------------------------------------------

        template<typename TRange, typename TPredicate>
        struct where_range : base_range
        {
            typedef                 where_range<TRange, TPredicate> this_type       ;
            typedef                 TRange                          range_type      ;
            typedef                 TPredicate                      predicate_type  ;

            typedef                 typename TRange::value_type     value_type      ;
            typedef                 typename TRange::return_type    return_type     ;
            enum
            {
                returns_reference   = TRange::returns_reference   ,
            };

            range_type              range       ;
            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD where_range (
                    range_type      range
                ,   predicate_type  predicate
                ) CPPLINQ_NOEXCEPT
                :   range       (std::move (range))
                ,   predicate   (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD where_range (where_range const & v)
                :   range       (v.range)
                ,   predicate   (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD where_range (where_range && v) CPPLINQ_NOEXCEPT
                :   range       (std::move (v.range))
                ,   predicate   (std::move (v.predicate))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return range.front ();
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                while (range.next ())
                {
                    if (predicate (range.front ()))
                    {
                        return true;
                    }
                }

                return false;
            }
        };

        template<typename TPredicate>
        struct where_builder : base_builder
        {
            typedef                 where_builder<TPredicate>   this_type       ;
            typedef                 TPredicate                  predicate_type  ;

            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD explicit where_builder (predicate_type predicate) CPPLINQ_NOEXCEPT
                :   predicate (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD where_builder (where_builder const & v)
                :   predicate (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD where_builder (where_builder && v) CPPLINQ_NOEXCEPT
                :   predicate (std::move (v.predicate))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD where_range<TRange, TPredicate> build (TRange range) const
            {
                return where_range<TRange, TPredicate>(std::move (range), predicate);
            }

        };

        // -------------------------------------------------------------------------

        // -------------------------------------------------------------------------

        template<typename TRange>
        struct take_range : base_range
        {
            typedef                 take_range<TRange>              this_type       ;
            typedef                 TRange                          range_type      ;

            typedef                 typename TRange::value_type     value_type      ;
            typedef                 typename TRange::return_type    return_type     ;
            enum
            {
                returns_reference   = TRange::returns_reference   ,
            };

            range_type              range       ;
            size_type               count       ;
            size_type               current     ;


            CPPLINQ_INLINEMETHOD take_range (
                    range_type      range
                ,   size_type       count
                ) CPPLINQ_NOEXCEPT
                :   range       (std::move (range))
                ,   count       (std::move (count))
                ,   current     (0)
            {
            }

            CPPLINQ_INLINEMETHOD take_range (take_range const & v)
                :   range       (v.range)
                ,   count       (v.count)
                ,   current     (v.current)
            {
            }

            CPPLINQ_INLINEMETHOD take_range (take_range && v) CPPLINQ_NOEXCEPT
                :   range       (std::move (v.range))
                ,   count       (std::move (v.count))
                ,   current     (std::move (v.current))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return range.front ();
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                if (current >= count)
                {
                    return false;
                }

                ++current;
                return range.next ();
            }
        };

        struct take_builder : base_builder
        {
            typedef                 take_builder        this_type       ;

            size_type               count       ;

            CPPLINQ_INLINEMETHOD explicit take_builder (size_type count) CPPLINQ_NOEXCEPT
                :   count (std::move (count))
            {
            }

            CPPLINQ_INLINEMETHOD take_builder (take_builder const & v) CPPLINQ_NOEXCEPT
                :   count (v.count)
            {
            }

            CPPLINQ_INLINEMETHOD take_builder (take_builder && v) CPPLINQ_NOEXCEPT
                :   count (std::move (v.count))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD take_range<TRange> build (TRange range) const
            {
                return take_range<TRange>(std::move (range), count);
            }

        };

        // -------------------------------------------------------------------------

        template<typename TRange, typename TPredicate>
        struct take_while_range : base_range
        {
            typedef                 take_while_range<TRange, TPredicate>    this_type       ;
            typedef                 TRange                                  range_type      ;
            typedef                 TPredicate                              predicate_type  ;

            typedef                 typename TRange::value_type             value_type      ;
            typedef                 typename TRange::return_type            return_type     ;
            enum
            {
                returns_reference   = TRange::returns_reference   ,
            };

            range_type              range       ;
            predicate_type          predicate   ;
            bool                    done        ;


            CPPLINQ_INLINEMETHOD take_while_range (
                    range_type      range
                ,   predicate_type  predicate
                ) CPPLINQ_NOEXCEPT
                :   range       (std::move (range))
                ,   predicate   (std::move (predicate))
                ,   done        (false)
            {
            }

            CPPLINQ_INLINEMETHOD take_while_range (take_while_range const & v)
                :   range       (v.range)
                ,   predicate   (v.predicate)
                ,   done        (v.done)
            {
            }

            CPPLINQ_INLINEMETHOD take_while_range (take_while_range && v) CPPLINQ_NOEXCEPT
                :   range       (std::move (v.range))
                ,   predicate   (std::move (v.predicate))
                ,   done        (std::move (v.done))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return range.front ();
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                if (done)
                {
                    return false;
                }

                if (!range.next ())
                {
                    done = true;
                    return false;
                }

                if (!predicate (range.front ()))
                {
                    done = true;
                    return false;
                }

                return true;
            }
        };

        template<typename TPredicate>
        struct take_while_builder : base_builder
        {
            typedef                 take_while_builder<TPredicate>  this_type       ;
            typedef                 TPredicate                      predicate_type  ;

            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD take_while_builder (predicate_type predicate) CPPLINQ_NOEXCEPT
                :   predicate (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD take_while_builder (take_while_builder const & v) CPPLINQ_NOEXCEPT
                :   predicate (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD take_while_builder (take_while_builder && v) CPPLINQ_NOEXCEPT
                :   predicate (std::move (v.predicate))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD take_while_range<TRange, TPredicate> build (TRange range) const
            {
                return take_while_range<TRange, TPredicate>(std::move (range), predicate);
            }

        };

        // -------------------------------------------------------------------------

        template<typename TRange>
        struct skip_range : base_range
        {
            typedef                 skip_range<TRange>              this_type       ;
            typedef                 TRange                          range_type      ;

            typedef                 typename TRange::value_type     value_type      ;
            typedef                 typename TRange::return_type    return_type     ;
            enum
            {
                returns_reference   = TRange::returns_reference   ,
            };

            range_type              range       ;
            size_type               count       ;
            size_type               current     ;

            CPPLINQ_INLINEMETHOD skip_range (
                    range_type      range
                ,   size_type       count
                ) CPPLINQ_NOEXCEPT
                :   range       (std::move (range))
                ,   count       (std::move (count))
                ,   current     (0)
            {
            }

            CPPLINQ_INLINEMETHOD skip_range (skip_range const & v)
                :   range       (v.range)
                ,   count       (v.count)
                ,   current     (v.current)
            {
            }

            CPPLINQ_INLINEMETHOD skip_range (skip_range && v) CPPLINQ_NOEXCEPT
                :   range       (std::move (v.range))
                ,   count       (std::move (v.count))
                ,   current     (std::move (v.current))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return range.front ();
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                if (current == invalid_size)
                {
                    return false;
                }

                while (current < count && range.next ())
                {
                    ++current;
                }

                if (current < count)
                {
                    current = invalid_size;
                    return false;
                }

                return range.next ();
            }
        };

        struct skip_builder : base_builder
        {
            typedef                 skip_builder        this_type       ;

            size_type               count       ;

            CPPLINQ_INLINEMETHOD explicit skip_builder (size_type count) CPPLINQ_NOEXCEPT
                :   count (std::move (count))
            {
            }

            CPPLINQ_INLINEMETHOD skip_builder (skip_builder const & v) CPPLINQ_NOEXCEPT
                :   count (v.count)
            {
            }

            CPPLINQ_INLINEMETHOD skip_builder (skip_builder && v) CPPLINQ_NOEXCEPT
                :   count (std::move (v.count))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD skip_range<TRange> build (TRange range) const
            {
                return skip_range<TRange>(std::move (range), count);
            }

        };

        // -------------------------------------------------------------------------

        template<typename TRange, typename TPredicate>
        struct skip_while_range : base_range
        {
            typedef                 skip_while_range<TRange, TPredicate>    this_type       ;
            typedef                 TRange                                  range_type      ;
            typedef                 TPredicate                              predicate_type  ;

            typedef                 typename TRange::value_type     value_type      ;
            typedef                 typename TRange::return_type    return_type     ;
            enum
            {
                returns_reference   = TRange::returns_reference   ,
            };

            range_type              range       ;
            predicate_type          predicate   ;
            bool                    skipping    ;

            CPPLINQ_INLINEMETHOD skip_while_range (
                    range_type      range
                ,   predicate_type  predicate
                ) CPPLINQ_NOEXCEPT
                :   range       (std::move (range))
                ,   predicate   (std::move (predicate))
                ,   skipping    (true)
            {
            }

            CPPLINQ_INLINEMETHOD skip_while_range (skip_while_range const & v)
                :   range       (v.range)
                ,   predicate   (v.predicate)
                ,   skipping    (v.skipping)
            {
            }

            CPPLINQ_INLINEMETHOD skip_while_range (skip_while_range && v) CPPLINQ_NOEXCEPT
                :   range       (std::move (v.range))
                ,   predicate   (std::move (v.predicate))
                ,   skipping    (std::move (v.skipping))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return range.front ();
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                if (!skipping)
                {
                    return range.next ();
                }

                while (range.next ())
                {
                    if (!predicate (range.front ()))
                    {
                        skipping = false;
                        return true;
                    }
                }

                return false;
            }
        };

        template <typename TPredicate>
        struct skip_while_builder : base_builder
        {
            typedef                 skip_while_builder<TPredicate>  this_type       ;
            typedef                 TPredicate                      predicate_type  ;

            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD skip_while_builder (predicate_type predicate) CPPLINQ_NOEXCEPT
                :   predicate (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD skip_while_builder (skip_while_builder const & v) CPPLINQ_NOEXCEPT
                :   predicate (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD skip_while_builder (skip_while_builder && v) CPPLINQ_NOEXCEPT
                :   predicate (std::move (v.predicate))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD skip_while_range<TRange, TPredicate> build (TRange range) const
            {
                return skip_while_range<TRange, TPredicate>(std::move (range), predicate);
            }

        };

        // -------------------------------------------------------------------------

        template<typename TRange>
        struct ref_range : base_range
        {
            typedef        std::reference_wrapper<
                                typename TRange::value_type const>  value_type  ;
            typedef                 value_type                      return_type ;
            enum
            {
                returns_reference   = 0   ,
            };

            typedef                 ref_range<TRange>               this_type   ;
            typedef                 TRange                          range_type  ;

            range_type              range       ;

            CPPLINQ_INLINEMETHOD ref_range (
                    range_type      range
                ) CPPLINQ_NOEXCEPT
                :   range       (std::move (range))
            {
                static_assert (
                        TRange::returns_reference
                    ,   "ref may only follow a range that returns references"
                    );
            }

            CPPLINQ_INLINEMETHOD ref_range (ref_range const & v)
                :   range       (v.range)
            {
            }

            CPPLINQ_INLINEMETHOD ref_range (ref_range && v) CPPLINQ_NOEXCEPT
                :   range       (std::move (v.range))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return value_type (range.front ());
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                return range.next ();
            }
        };

        struct ref_builder : base_builder
        {
            typedef                 ref_builder this_type   ;

            CPPLINQ_INLINEMETHOD ref_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD ref_builder (ref_builder const & v)
            {
            }

            CPPLINQ_INLINEMETHOD ref_builder (ref_builder && v) CPPLINQ_NOEXCEPT
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD ref_range<TRange> build (TRange range) const
            {
                return ref_range<TRange>(std::move (range));
            }

        };

        // -------------------------------------------------------------------------

        // -------------------------------------------------------------------------

        template<typename TRange, typename TPredicate>
        struct select_range : base_range
        {
            static typename TRange::value_type get_source ();
            static          TPredicate get_predicate ();


            typedef        decltype (get_predicate ()(get_source ()))   raw_value_type  ;
            typedef        typename cleanup_type<raw_value_type>::type  value_type      ;
            typedef                 value_type                          return_type     ;
            enum
            {
                returns_reference   = 0   ,
            };

            typedef                 select_range<TRange, TPredicate>    this_type       ;
            typedef                 TRange                              range_type      ;
            typedef                 TPredicate                          predicate_type  ;

            range_type              range       ;
            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD select_range (
                    range_type      range
                ,   predicate_type  predicate
                ) CPPLINQ_NOEXCEPT
                :   range       (std::move (range))
                ,   predicate   (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD select_range (select_range const & v)
                :   range       (v.range)
                ,   predicate   (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD select_range (select_range && v) CPPLINQ_NOEXCEPT
                :   range       (std::move (v.range))
                ,   predicate   (std::move (v.predicate))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return predicate (range.front ());
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                return range.next ();
            }
        };

        template<typename TPredicate>
        struct select_builder : base_builder
        {
            typedef                 select_builder<TPredicate>  this_type       ;
            typedef                 TPredicate                  predicate_type  ;

            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD explicit select_builder (predicate_type predicate) CPPLINQ_NOEXCEPT
                :   predicate (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD select_builder (select_builder const & v)
                :   predicate (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD select_builder (select_builder && v) CPPLINQ_NOEXCEPT
                :   predicate (std::move (v.predicate))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD select_range<TRange, TPredicate> build (TRange range) const
            {
                return select_range<TRange, TPredicate>(std::move (range), predicate);
            }

        };

        // -------------------------------------------------------------------------

        // Some trickery in order to force the code to compile on VS2012
        template<typename TRange, typename TPredicate>
        struct select_many_range_helper
        {
            static typename TRange::value_type get_source ();
            static          TPredicate get_predicate ();

            typedef        decltype (get_predicate ()(get_source ()))           raw_inner_range_type    ;
            typedef        typename cleanup_type<raw_inner_range_type>::type    inner_range_type        ;

            static         inner_range_type get_inner_range ();

            typedef        decltype (get_inner_range ().front ())               raw_value_type          ;
            typedef        typename cleanup_type<raw_value_type>::type          value_type              ;

        };

        template<typename TRange, typename TPredicate>
        struct select_many_range : base_range
        {
            typedef select_many_range_helper<TRange, TPredicate>    helper_type         ;

            typedef        typename helper_type::inner_range_type   inner_range_type    ;
            typedef        typename helper_type::value_type         value_type          ;
            typedef                 value_type                      return_type         ;
            enum
            {
                returns_reference   = 0   ,
            };

            typedef                 select_many_range<TRange, TPredicate>       this_type               ;
            typedef                 TRange                                      range_type              ;
            typedef                 TPredicate                                  predicate_type          ;

            range_type              range       ;
            predicate_type          predicate   ;

            opt<inner_range_type>   inner_range ;


            CPPLINQ_INLINEMETHOD select_many_range (
                    range_type      range
                ,   predicate_type  predicate
                ) CPPLINQ_NOEXCEPT
                :   range       (std::move (range))
                ,   predicate   (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD select_many_range (select_many_range const & v)
                :   range       (v.range)
                ,   predicate   (v.predicate)
                ,   inner_range (v.inner_range)
            {
            }

            CPPLINQ_INLINEMETHOD select_many_range (select_many_range && v) CPPLINQ_NOEXCEPT
                :   range       (std::move (v.range))
                ,   predicate   (std::move (v.predicate))
                ,   inner_range (std::move (v.inner_range))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                assert (inner_range);
                return inner_range->front ();
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                if (inner_range && inner_range->next ())
                {
                    return true;
                }

                if (range.next ())
                {
                    inner_range = predicate (range.front ());
                    return inner_range && inner_range->next ();
                }

                return false;
            }
        };

        template<typename TPredicate>
        struct select_many_builder : base_builder
        {
            typedef                 select_many_builder<TPredicate> this_type       ;
            typedef                 TPredicate                      predicate_type  ;

            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD explicit select_many_builder (predicate_type predicate) CPPLINQ_NOEXCEPT
                :   predicate (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD select_many_builder (select_many_builder const & v)
                :   predicate (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD select_many_builder (select_many_builder && v) CPPLINQ_NOEXCEPT
                :   predicate (std::move (v.predicate))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD select_many_range<TRange, TPredicate> build (TRange range) const
            {
                return select_many_range<TRange, TPredicate>(std::move (range), predicate);
            }

        };

        // -------------------------------------------------------------------------

        template<
                typename TRange
            ,   typename TOtherRange
            ,   typename TKeySelector
            ,   typename TOtherKeySelector
            ,   typename TCombiner
            >
        struct join_range : base_range
        {
            static typename TRange::value_type      get_source ()               ;
            static typename TOtherRange::value_type get_other_source ()         ;
            static          TKeySelector            get_key_selector ()         ;
            static          TOtherKeySelector       get_other_key_selector ()   ;
            static          TCombiner               get_combiner ()             ;

            typedef         decltype (get_key_selector () (get_source ()))      raw_key_type    ;
            typedef         typename cleanup_type<raw_key_type>::type           key_type        ;

            typedef         decltype (get_other_key_selector () (get_other_source ()))
                                                                                raw_other_key_type  ;
            typedef         typename cleanup_type<raw_other_key_type>::type     other_key_type      ;

            typedef         decltype (get_combiner () (get_source (), get_other_source ()))
                                                                                raw_value_type  ;
            typedef         typename cleanup_type<raw_value_type>::type         value_type      ;
            typedef                 value_type                                  return_type     ;
            enum
            {
                returns_reference   = 0   ,
            };

            typedef                 join_range<
                    TRange
                ,   TOtherRange
                ,   TKeySelector
                ,   TOtherKeySelector
                ,   TCombiner
                >                                               this_type               ;
            typedef                 TRange                      range_type              ;
            typedef                 TOtherRange                 other_range_type        ;
            typedef                 TKeySelector                key_selector_type       ;
            typedef                 TOtherKeySelector           other_key_selector_type ;
            typedef                 TCombiner                   combiner_type           ;
            typedef                 std::multimap<
                                            other_key_type
                                        ,   typename TOtherRange::value_type
                                        >                       map_type                ;
            typedef     typename    map_type::const_iterator    map_iterator_type       ;

            range_type                  range               ;
            other_range_type            other_range         ;
            key_selector_type           key_selector        ;
            other_key_selector_type     other_key_selector  ;
            combiner_type               combiner            ;

            bool                        start               ;
            map_type                    map                 ;
            map_iterator_type           current             ;

            CPPLINQ_INLINEMETHOD join_range (
                    range_type              range
                ,   other_range_type        other_range
                ,   key_selector_type       key_selector
                ,   other_key_selector_type other_key_selector
                ,   combiner_type           combiner
                ) CPPLINQ_NOEXCEPT
                :   range              (std::move (range))
                ,   other_range        (std::move (other_range))
                ,   key_selector       (std::move (key_selector))
                ,   other_key_selector (std::move (other_key_selector))
                ,   combiner           (std::move (combiner))
                ,   start              (true)
            {
            }

            CPPLINQ_INLINEMETHOD join_range (join_range const & v)
                :   range              (v.range)
                ,   other_range        (v.other_range)
                ,   key_selector       (v.key_selector)
                ,   other_key_selector (v.other_key_selector)
                ,   combiner           (v.combiner)
                ,   start              (v.start)
                ,   map                (v.map)
                ,   current            (v.current)
            {
            }

            CPPLINQ_INLINEMETHOD join_range (join_range && v) CPPLINQ_NOEXCEPT
                :   range              (std::move (v.range))
                ,   other_range        (std::move (v.other_range))
                ,   key_selector       (std::move (v.key_selector))
                ,   other_key_selector (std::move (v.other_key_selector))
                ,   combiner           (std::move (v.combiner))
                ,   start              (std::move (v.start))
                ,   map                (std::move (v.map))
                ,   current            (std::move (v.current))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                assert (current != map.end ());
                return combiner (range.front (), current->second);
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                if (start)
                {
                    start = false;
                    while (other_range.next ())
                    {
                        auto other_value    = other_range.front ();
                        auto other_key      = other_key_selector (other_value);
                        map.insert (typename map_type::value_type (std::move (other_key), std::move (other_value)));
                    }

                    current = map.end ();
                    if (map.size () == 0U)
                    {
                        return false;
                    }
                }

                if (current != map.end ())
                {
                    auto previous = current;
                    ++current;
                    if (current != map.end () && !(previous->first < current->first))
                    {
                        return true;
                    }
                }

                while (range.next ())
                {
                    auto value  = range.front ();
                    auto key    = key_selector (value);

                    current     = map.find (key);
                    if (current != map.end ())
                    {
                        return true;
                    }
                }

                return false;
            }
        };

        template<
                typename TOtherRange
            ,   typename TKeySelector
            ,   typename TOtherKeySelector
            ,   typename TCombiner
            >
        struct join_builder : base_builder
        {
            typedef                 join_builder<
                    TOtherRange
                ,   TKeySelector
                ,   TOtherKeySelector
                ,   TCombiner
                >                                               this_type               ;

            typedef                 TOtherRange                 other_range_type        ;
            typedef                 TKeySelector                key_selector_type       ;
            typedef                 TOtherKeySelector           other_key_selector_type ;
            typedef                 TCombiner                   combiner_type           ;

            other_range_type        other_range         ;
            key_selector_type       key_selector        ;
            other_key_selector_type other_key_selector  ;
            combiner_type           combiner            ;

            CPPLINQ_INLINEMETHOD join_builder (
                    other_range_type        other_range
                ,   key_selector_type       key_selector
                ,   other_key_selector_type other_key_selector
                ,   combiner_type           combiner
                ) CPPLINQ_NOEXCEPT
                :   other_range        (std::move (other_range))
                ,   key_selector       (std::move (key_selector))
                ,   other_key_selector (std::move (other_key_selector))
                ,   combiner           (std::move (combiner))
            {
            }

            CPPLINQ_INLINEMETHOD join_builder (join_builder const & v)
                :   other_range        (v.other_range)
                ,   key_selector       (v.key_selector)
                ,   other_key_selector (v.other_key_selector)
                ,   combiner           (v.combiner)
            {
            }

            CPPLINQ_INLINEMETHOD join_builder (join_builder && v) CPPLINQ_NOEXCEPT
                :   other_range        (std::move (v.other_range))
                ,   key_selector       (std::move (v.key_selector))
                ,   other_key_selector (std::move (v.other_key_selector))
                ,   combiner           (std::move (v.combiner))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD join_range<TRange, TOtherRange, TKeySelector, TOtherKeySelector, TCombiner> build (TRange range) const
            {
                return join_range<TRange, TOtherRange, TKeySelector, TOtherKeySelector, TCombiner> (
                        std::move (range)
                    ,   other_range
                    ,   key_selector
                    ,   other_key_selector
                    ,   combiner
                    );
            }
        };


        // -------------------------------------------------------------------------

        template<typename TRange>
        struct distinct_range : base_range
        {
            typedef             distinct_range<TRange>                          this_type           ;
            typedef             TRange                                          range_type          ;

            typedef    typename cleanup_type<typename TRange::value_type>::type value_type          ;
            typedef             value_type const &                              return_type         ;
            enum
            {
                returns_reference   = 1 ,
            };

            typedef             std::set<value_type>               set_type                         ;
            typedef    typename set_type::const_iterator           set_iterator_type                ;

            range_type                  range               ;
            set_type                    set                 ;
            set_iterator_type           current             ;

            CPPLINQ_INLINEMETHOD distinct_range (
                        range_type          range
                ) CPPLINQ_NOEXCEPT
                :   range               (std::move (range))
            {
            }

            CPPLINQ_INLINEMETHOD distinct_range (distinct_range const & v) CPPLINQ_NOEXCEPT
                :   range               (v.range)
                ,   set                 (v.set)
                ,   current             (v.current)
            {
            }

            CPPLINQ_INLINEMETHOD distinct_range (distinct_range && v) CPPLINQ_NOEXCEPT
                :   range               (std::move (v.range))
                ,   set                 (std::move (v.set))
                ,   current             (std::move (v.current))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return *current;
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                while (range.next ())
                {
                    auto result = set.insert (range.front ());
                    if (result.second)
                    {
                        current = result.first;
                        return true;
                    }
                }

                return false;
            }
        };

        struct distinct_builder : base_builder
        {
            typedef                 distinct_builder                    this_type               ;

            CPPLINQ_INLINEMETHOD distinct_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD distinct_builder (distinct_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD distinct_builder (distinct_builder && v) CPPLINQ_NOEXCEPT
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD distinct_range<TRange> build (TRange range) const
            {
                return distinct_range<TRange> (std::move (range));
            }
        };

        // -------------------------------------------------------------------------

        template<typename TRange, typename TOtherRange>
        struct union_range : base_range
        {
            typedef             union_range<TRange, TOtherRange>                this_type           ;
            typedef             TRange                                          range_type          ;
            typedef             TOtherRange                                     other_range_type    ;

            typedef    typename cleanup_type<typename TRange::value_type>::type value_type          ;
            typedef             value_type const &                              return_type         ;
            enum
            {
                returns_reference   = 1 ,
            };

            typedef             std::set<value_type>               set_type                         ;
            typedef    typename set_type::const_iterator           set_iterator_type                ;


            range_type                  range               ;
            other_range_type            other_range         ;
            set_type                    set                 ;
            set_iterator_type           current             ;

            CPPLINQ_INLINEMETHOD union_range (
                        range_type          range
                    ,   other_range_type    other_range
                ) CPPLINQ_NOEXCEPT
                :   range               (std::move (range))
                ,   other_range         (std::move (other_range))
            {
            }

            CPPLINQ_INLINEMETHOD union_range (union_range const & v) CPPLINQ_NOEXCEPT
                :   range               (v.range)
                ,   other_range         (v.other_range)
                ,   set                 (v.set)
                ,   current             (v.current)
            {
            }

            CPPLINQ_INLINEMETHOD union_range (union_range && v) CPPLINQ_NOEXCEPT
                :   range               (std::move (v.range))
                ,   other_range         (std::move (v.other_range))
                ,   set                 (std::move (v.set))
                ,   current             (std::move (v.current))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return *current;
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                while (range.next ())
                {
                    auto result = set.insert (range.front ());
                    if (result.second)
                    {
                        current = result.first;
                        return true;
                    }
                }

                while (other_range.next ())
                {
                    auto result = set.insert (other_range.front ());
                    if (result.second)
                    {
                        current = result.first;
                        return true;
                    }
                }

                return false;
            }
        };

        template <typename TOtherRange>
        struct union_builder : base_builder
        {
            typedef                 union_builder<TOtherRange>              this_type       ;
            typedef                 TOtherRange                             other_range_type;

            other_range_type        other_range         ;

            CPPLINQ_INLINEMETHOD union_builder (TOtherRange other_range) CPPLINQ_NOEXCEPT
                : other_range (std::move (other_range))
            {
            }

            CPPLINQ_INLINEMETHOD union_builder (union_builder const & v) CPPLINQ_NOEXCEPT
                : other_range (v.other_range)
            {
            }

            CPPLINQ_INLINEMETHOD union_builder (union_builder && v) CPPLINQ_NOEXCEPT
                : other_range (std::move (v.other_range))
            {
            }

            template <typename TRange>
            CPPLINQ_INLINEMETHOD union_range<TRange, TOtherRange> build (TRange range) const
            {
                return union_range<TRange, TOtherRange> (std::move (range), std::move (other_range));
            }
        };

        // -------------------------------------------------------------------------

        template<typename TRange, typename TOtherRange>
        struct intersect_range : base_range
        {
            typedef             intersect_range<TRange, TOtherRange>            this_type           ;
            typedef             TRange                                          range_type          ;
            typedef             TOtherRange                                     other_range_type    ;

            typedef    typename cleanup_type<typename TRange::value_type>::type value_type          ;
            typedef             value_type const &                              return_type         ;
            enum
            {
                returns_reference   = 1 ,
            };

            typedef             std::set<value_type>               set_type                         ;
            typedef    typename set_type::const_iterator           set_iterator_type                ;


            range_type                  range               ;
            other_range_type            other_range         ;
            set_type                    set                 ;
            set_iterator_type           current             ;
            bool                        start               ;

            CPPLINQ_INLINEMETHOD intersect_range (
                        range_type          range
                    ,   other_range_type    other_range
                ) CPPLINQ_NOEXCEPT
                :   range               (std::move (range))
                ,   other_range         (std::move (other_range))
                ,   start               (true)
            {
            }

            CPPLINQ_INLINEMETHOD intersect_range (intersect_range const & v) CPPLINQ_NOEXCEPT
                :   range               (v.range)
                ,   other_range         (v.other_range)
                ,   set                 (v.set)
                ,   current             (v.current)
                ,   start               (v.start)
            {
            }

            CPPLINQ_INLINEMETHOD intersect_range (intersect_range && v) CPPLINQ_NOEXCEPT
                :   range               (std::move (v.range))
                ,   other_range         (std::move (v.other_range))
                ,   set                 (std::move (v.set))
                ,   current             (std::move (v.current))
                ,   start               (std::move (v.start))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                assert (!start);
                return *current;
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                if (start)
                {
                    start = false;

                    while (other_range.next ())
                    {
                        set.insert (other_range.front ());
                    }

                    while (range.next ())
                    {
                        current = set.find (range.front ());
                        if (current != set.end ())
                        {
                            return true;
                        }
                    }

                    set.clear ();

                    return false;
                }

                if (set.empty ())
                {
                    return false;
                }

                set.erase (current);

                while (range.next ())
                {
                    current = set.find (range.front ());
                    if (current != set.end ())
                    {
                        return true;
                    }
                }

                return false;
            }
        };

        template <typename TOtherRange>
        struct intersect_builder : base_builder
        {
            typedef                 intersect_builder<TOtherRange>          this_type       ;
            typedef                 TOtherRange                             other_range_type;

            other_range_type        other_range         ;

            CPPLINQ_INLINEMETHOD intersect_builder (TOtherRange other_range) CPPLINQ_NOEXCEPT
                : other_range (std::move (other_range))
            {
            }

            CPPLINQ_INLINEMETHOD intersect_builder (intersect_builder const & v) CPPLINQ_NOEXCEPT
                : other_range (v.other_range)
            {
            }

            CPPLINQ_INLINEMETHOD intersect_builder (intersect_builder && v) CPPLINQ_NOEXCEPT
                : other_range (std::move (v.other_range))
            {
            }

            template <typename TRange>
            CPPLINQ_INLINEMETHOD intersect_range<TRange, TOtherRange> build (TRange range) const
            {
                return intersect_range<TRange, TOtherRange> (std::move (range), std::move (other_range));
            }
        };

        // -------------------------------------------------------------------------

        template<typename TRange, typename TOtherRange>
        struct except_range : base_range
        {
            typedef             except_range<TRange, TOtherRange>               this_type           ;
            typedef             TRange                                          range_type          ;
            typedef             TOtherRange                                     other_range_type    ;

            typedef    typename cleanup_type<typename TRange::value_type>::type value_type          ;
            typedef             value_type const &                              return_type         ;
            enum
            {
                returns_reference   = 1 ,
            };

            typedef             std::set<value_type>               set_type                         ;
            typedef    typename set_type::const_iterator           set_iterator_type                ;

            range_type                  range               ;
            other_range_type            other_range         ;
            set_type                    set                 ;
            set_iterator_type           current             ;
            bool                        start               ;

            CPPLINQ_INLINEMETHOD except_range (
                        range_type          range
                    ,   other_range_type    other_range
                ) CPPLINQ_NOEXCEPT
                :   range               (std::move (range))
                ,   other_range         (std::move (other_range))
                ,   start               (true)
            {
            }

            CPPLINQ_INLINEMETHOD except_range (except_range const & v) CPPLINQ_NOEXCEPT
                :   range               (v.range)
                ,   other_range         (v.other_range)
                ,   set                 (v.set)
                ,   current             (v.current)
                ,   start               (v.start)
            {
            }

            CPPLINQ_INLINEMETHOD except_range (except_range && v) CPPLINQ_NOEXCEPT
                :   range               (std::move (v.range))
                ,   other_range         (std::move (v.other_range))
                ,   set                 (std::move (v.set))
                ,   current             (std::move (v.current))
                ,   start               (std::move (v.start))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return *current;
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                if (start)
                {
                    start = false;
                    while (other_range.next ())
                    {
                        set.insert (other_range.front ());
                    }
                }

                while (range.next ())
                {
                    auto result = set.insert (range.front ());
                    if (result.second)
                    {
                        current = result.first;
                        return true;
                    }
                }

                return false;
            }
        };

        template <typename TOtherRange>
        struct except_builder : base_builder
        {
            typedef                 union_builder<TOtherRange>              this_type       ;
            typedef                 TOtherRange                             other_range_type;

            other_range_type        other_range         ;

            CPPLINQ_INLINEMETHOD except_builder (TOtherRange other_range) CPPLINQ_NOEXCEPT
                : other_range (std::move (other_range))
            {
            }

            CPPLINQ_INLINEMETHOD except_builder (except_builder const & v) CPPLINQ_NOEXCEPT
                : other_range (v.other_range)
            {
            }

            CPPLINQ_INLINEMETHOD except_builder (except_builder && v) CPPLINQ_NOEXCEPT
                : other_range (std::move (v.other_range))
            {
            }

            template <typename TRange>
            CPPLINQ_INLINEMETHOD except_range<TRange, TOtherRange> build (TRange range) const
            {
                return except_range<TRange, TOtherRange> (std::move (range), std::move (other_range));
            }
        };

        // -------------------------------------------------------------------------

        template<typename TRange, typename TOtherRange>
        struct concat_range : base_range
        {
            typedef             concat_range<TRange, TOtherRange>                       this_type           ;
            typedef             TRange                                                  range_type          ;
            typedef             TOtherRange                                             other_range_type    ;

            typedef typename    cleanup_type<typename TRange::value_type>::type         value_type          ;
            typedef typename    cleanup_type<typename TOtherRange::value_type>::type    other_value_type    ;
            typedef             value_type                                              return_type         ;

            enum
            {
                returns_reference   = 0         ,
            };

            enum state
            {
                state_initial                   ,
                state_iterating_range           ,
                state_iterating_other_range     ,
                state_end                       ,
            };

            range_type                  range               ;
            other_range_type            other_range         ;
            state                       state               ;

            CPPLINQ_INLINEMETHOD concat_range (
                        range_type          range
                    ,   other_range_type    other_range
                ) CPPLINQ_NOEXCEPT
                :   range               (std::move (range))
                ,   other_range         (std::move (other_range))
                ,   state               (state_initial)
            {
            }

            CPPLINQ_INLINEMETHOD concat_range (concat_range const & v) CPPLINQ_NOEXCEPT
                :   range               (v.range)
                ,   other_range         (v.other_range)
                ,   state               (v.state)
            {
            }

            CPPLINQ_INLINEMETHOD concat_range (concat_range && v) CPPLINQ_NOEXCEPT
                :   range               (std::move (v.range))
                ,   other_range         (std::move (v.other_range))
                ,   state               (std::move (v.state))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                switch (state)
                {
                case state_initial:
                case state_end:
                default:
                    assert (false);       // Intentionally falls through
                case state_iterating_range:
                    return range.front ();
                case state_iterating_other_range:
                    return other_range.front ();
                };
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                switch (state)
                {
                case state_initial:
                    if (range.next ())
                    {
                        state = state_iterating_range;
                        return true;
                    }

                    if (other_range.next ())
                    {
                        state = state_iterating_other_range;
                        return true;
                    }

                    return false;
                case state_iterating_range:
                    if (range.next ())
                    {
                        return true;
                    }

                    if (other_range.next ())
                    {
                        state = state_iterating_other_range;
                        return true;
                    }

                    return false;
                case state_iterating_other_range:
                    if (other_range.next ())
                    {
                        return true;
                    }

                    return false;
                case state_end:
                default:
                    return false;
                }
            }
        };

        template <typename TOtherRange>
        struct concat_builder : base_builder
        {
            typedef                 concat_builder<TOtherRange>             this_type       ;
            typedef                 TOtherRange                             other_range_type;

            other_range_type        other_range         ;

            CPPLINQ_INLINEMETHOD concat_builder (TOtherRange other_range) CPPLINQ_NOEXCEPT
                : other_range (std::move (other_range))
            {
            }

            CPPLINQ_INLINEMETHOD concat_builder (concat_builder const & v) CPPLINQ_NOEXCEPT
                : other_range (v.other_range)
            {
            }

            CPPLINQ_INLINEMETHOD concat_builder (concat_builder && v) CPPLINQ_NOEXCEPT
                : other_range (std::move (v.other_range))
            {
            }

            template <typename TRange>
            CPPLINQ_INLINEMETHOD concat_range<TRange, TOtherRange> build (TRange range) const
            {
                return concat_range<TRange, TOtherRange> (std::move (range), std::move (other_range));
            }
        };

        // -------------------------------------------------------------------------

        namespace experimental
        {
            // -------------------------------------------------------------------------

            // TODO: Verify that container range aggregator has the right semantics

            template<typename TRange>
            struct container_iterator
            {
                typedef                 std::forward_iterator_tag   iterator_category   ;
                typedef     typename    TRange::value_type          value_type          ;
                typedef     typename    TRange::return_type         return_type         ;
                enum
                {
                    returns_reference   = TRange::returns_reference   ,
                };

                typedef                 std::ptrdiff_t              difference_type     ;
                typedef                 value_type*                 pointer             ;
                typedef                 value_type&                 reference           ;

                typedef                 container_iterator<TRange>  this_type   ;
                typedef                 TRange                      range_type  ;

                bool                    has_value                               ;
                opt<range_type>         range                                   ;

                CPPLINQ_INLINEMETHOD container_iterator ()   CPPLINQ_NOEXCEPT
                    :   has_value   (false)
                {
                }

                CPPLINQ_INLINEMETHOD container_iterator (range_type r)   CPPLINQ_NOEXCEPT
                    :   range      (std::move (r))
                {
                    has_value = range && range->next ();
                }

                CPPLINQ_INLINEMETHOD container_iterator (container_iterator const & v) CPPLINQ_NOEXCEPT
                    :   has_value   (v.has_value)
                    ,   range       (v.range)
                {
                }

                CPPLINQ_INLINEMETHOD container_iterator (container_iterator && v) CPPLINQ_NOEXCEPT
                    :   has_value   (std::move (v.has_value))
                    ,   range       (std::move (v.range))
                {
                }

                CPPLINQ_INLINEMETHOD return_type        operator* () const
                {
                    assert (has_value);
                    assert (range);
                    return range->front ();
                }

                CPPLINQ_INLINEMETHOD value_type const * operator-> () const
                {
                    static_assert (
                            returns_reference
                        ,   "operator-> requires a range that returns a reference, typically select causes ranges to return values not references"
                        );
                    return &range->front ();
                }

                CPPLINQ_INLINEMETHOD this_type & operator++()
                {
                    if (has_value && range)
                    {
                        has_value = range->next ();
                    }

                    return *this;
                }

                CPPLINQ_INLINEMETHOD bool operator== (this_type const & v) const CPPLINQ_NOEXCEPT
                {
                    if (!has_value && !v.has_value)
                    {
                        return true;
                    }
                    else if (has_value && has_value && range.get_ptr () == v.range.get_ptr ())
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }

                CPPLINQ_INLINEMETHOD bool operator!= (this_type const & v) const CPPLINQ_NOEXCEPT
                {
                    return !(*this == v);
                }
            };

            template<typename TRange>
            struct container
            {
                typedef                 container<TRange>   this_type                   ;
                typedef                 TRange              range_type                  ;
                typedef                 typename TRange::value_type     value_type      ;
                typedef                 typename TRange::return_type    return_type     ;
                enum
                {
                    returns_reference   = TRange::returns_reference   ,
                };

                range_type              range   ;

                CPPLINQ_INLINEMETHOD explicit container (TRange range)
                    :   range (std::move (range))
                {
                }

                CPPLINQ_INLINEMETHOD container (container const & v) CPPLINQ_NOEXCEPT
                    :   range       (v.range)
                {
                }

                CPPLINQ_INLINEMETHOD container (container && v) CPPLINQ_NOEXCEPT
                    :   range       (std::move (v.range))
                {
                }

                CPPLINQ_INLINEMETHOD container_iterator<TRange>  begin () CPPLINQ_NOEXCEPT
                {
                    return container_iterator<TRange>(range);
                }

                CPPLINQ_INLINEMETHOD container_iterator<TRange>  end () CPPLINQ_NOEXCEPT
                {
                    return container_iterator<TRange>();
                }

            };

            struct container_builder : base_builder
            {
                typedef                 container_builder       this_type       ;

                CPPLINQ_INLINEMETHOD container_builder () CPPLINQ_NOEXCEPT
                {
                }

                CPPLINQ_INLINEMETHOD container_builder (container_builder const & v) CPPLINQ_NOEXCEPT
                {
                }

                CPPLINQ_INLINEMETHOD container_builder (container_builder && v) CPPLINQ_NOEXCEPT
                {
                }

                template<typename TRange>
                CPPLINQ_METHOD container<TRange> build (TRange range) const
                {
                    return container<TRange> (std::move (range));
                }

            };
        }

        // -------------------------------------------------------------------------

        struct to_vector_builder : base_builder
        {
            typedef                 to_vector_builder       this_type       ;

            size_type               capacity;

            CPPLINQ_INLINEMETHOD explicit to_vector_builder (size_type capacity = 16U) CPPLINQ_NOEXCEPT
                :   capacity    (capacity)
            {
            }

            CPPLINQ_INLINEMETHOD to_vector_builder (to_vector_builder const & v) CPPLINQ_NOEXCEPT
                :   capacity (v.capacity)
            {
            }

            CPPLINQ_INLINEMETHOD to_vector_builder (to_vector_builder && v) CPPLINQ_NOEXCEPT
                :   capacity (std::move (v.capacity))
            {
            }

            template<typename TRange>
            CPPLINQ_METHOD std::vector<typename TRange::value_type> build (TRange range) const
            {
                std::vector<typename TRange::value_type> result;
                result.reserve (capacity);

                while (range.next ())
                {
                    result.push_back (range.front ());
                }

                return result;
            }

        };

        struct to_list_builder : base_builder
        {
            typedef                 to_list_builder       this_type       ;

            CPPLINQ_INLINEMETHOD explicit to_list_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD to_list_builder (to_list_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD to_list_builder (to_list_builder && v) CPPLINQ_NOEXCEPT
            {
            }

            template<typename TRange>
            CPPLINQ_METHOD std::list<typename TRange::value_type> build (TRange range) const
            {
                std::list<typename TRange::value_type> result;

                while (range.next ())
                {
                    result.push_back (range.front ());
                }

                return result;
            }

        };

        // -------------------------------------------------------------------------

        template<typename TKeyPredicate>
        struct to_map_builder : base_builder
        {
            static TKeyPredicate get_key_predicate ();

            typedef                     to_map_builder<TKeyPredicate>   this_type           ;
            typedef                     TKeyPredicate                   key_predicate_type  ;

            key_predicate_type          key_predicate   ;

            CPPLINQ_INLINEMETHOD explicit to_map_builder (key_predicate_type key_predicate) CPPLINQ_NOEXCEPT
                :   key_predicate   (std::move (key_predicate))
            {
            }

            CPPLINQ_INLINEMETHOD to_map_builder (to_map_builder const & v)
                :   key_predicate (v.key_predicate)
            {
            }

            CPPLINQ_INLINEMETHOD to_map_builder (to_map_builder && v) CPPLINQ_NOEXCEPT
                :   key_predicate (std::move (v.key_predicate))
            {
            }

            template<typename TRange>
            CPPLINQ_METHOD std::map<
                    typename get_transformed_type<key_predicate_type, typename TRange::value_type>::type
                ,   typename TRange::value_type
                > build (TRange range) const
            {
                typedef std::map<
                    typename get_transformed_type<key_predicate_type, typename TRange::value_type>::type
                ,   typename TRange::value_type
                >   result_type;

                result_type result;

                while (range.next ())
                {
                    auto v = range.front ();
                    auto k = key_predicate (v);

                    result.insert (typename result_type::value_type (std::move (k), std::move (v)));
                }

                return result;
            }

        };

        // -------------------------------------------------------------------------

        template<typename TKey, typename TValue>
        struct lookup
        {
            typedef             TKey    key_type    ;
            typedef             TValue  value_type  ;

            typedef             std::vector<std::pair<key_type, size_type>>     keys_type           ;
            typedef             std::vector<value_type>                         values_type         ;

            typedef typename    values_type::const_iterator                     values_iterator_type;

            template<typename TRange, typename TSelector>
            CPPLINQ_METHOD lookup (size_type capacity, TRange range, TSelector selector)
            {
                keys_type   k;
                values_type v;
                k.reserve (capacity);
                v.reserve (capacity);

                auto index = 0U;
                while (range.next ())
                {
                    auto value  = range.front ();
                    auto key    = selector (value);
                    v.push_back (std::move (value));
                    k.push_back (typename keys_type::value_type (std::move (key), index));
                    ++index;
                }

                if (v.size () == 0)
                {
                    return;
                }

                std::sort (
                        k.begin ()
                    ,   k.end ()
                    ,   [] (typename keys_type::value_type const & l, typename keys_type::value_type const & r)
                        {
                            return l.first < r.first;
                        }
                    );

                keys.reserve (k.size ());
                values.reserve (v.size ());

                auto iter       = k.begin ();
                auto end        = k.end ();

                index = 0U;

                if (iter != end)
                {
                    values.push_back (std::move (v[iter->second]));
                    keys.push_back (typename keys_type::value_type (iter->first, index));
                }

                auto previous   = iter;
                ++iter;
                ++index;

                while (iter != end)
                {
                    values.push_back (v[iter->second]);

                    if (previous->first < iter->first)
                    {
                        keys.push_back (typename keys_type::value_type (iter->first, index));
                    }

                    previous = iter;
                    ++iter;
                    ++index;
                }
            }

            CPPLINQ_INLINEMETHOD lookup (lookup const & v)
                :   values  (v.values)
                ,   keys    (v.keys)
            {
            }

            CPPLINQ_INLINEMETHOD lookup (lookup && v) CPPLINQ_NOEXCEPT
                :   values  (std::move (v.values))
                ,   keys    (std::move (v.keys))
            {
            }

            CPPLINQ_INLINEMETHOD void swap (lookup & v) CPPLINQ_NOEXCEPT
            {
                values.swap (v.values);
                keys.swap (v.keys);
            }

            CPPLINQ_INLINEMETHOD lookup & operator= (lookup const & v)
            {
                if (this == std::addressof (v))
                {
                    return *this;
                }

                lookup tmp (v);

                swap (tmp);

                return *this;
            }

            CPPLINQ_INLINEMETHOD lookup & operator= (lookup && v) CPPLINQ_NOEXCEPT
            {
                if (this == std::addressof (v))
                {
                    return *this;
                }

                swap (v);

                return *this;
            }

            struct lookup_range : base_range
            {
                typedef         lookup_range                this_type       ;

                enum
                {
                    returns_reference = 1 ,
                };

                typedef         TValue                      value_type      ;
                typedef         value_type const &          return_type     ;

                enum state
                {
                    state_initial   ,
                    state_iterating ,
                    state_end       ,
                };

                values_type const *     values  ;
                size_type               iter    ;
                size_type               end     ;
                state                   state   ;

                CPPLINQ_INLINEMETHOD lookup_range (
                        values_type const *     values
                    ,   size_type               iter
                    ,   size_type               end
                    ) CPPLINQ_NOEXCEPT
                    :   values  (values)
                    ,   iter    (iter)
                    ,   end     (end)
                    ,   state   (state_initial)
                {
                    assert (values);
                }

                CPPLINQ_INLINEMETHOD lookup_range (lookup_range const & v) CPPLINQ_NOEXCEPT
                    :   values  (v.values)
                    ,   iter    (v.iter)
                    ,   end     (v.end)
                    ,   state   (v.state)
                {
                }

                CPPLINQ_INLINEMETHOD lookup_range (lookup_range && v) CPPLINQ_NOEXCEPT
                    :   values  (std::move (v.values))
                    ,   iter    (std::move (v.iter))
                    ,   end     (std::move (v.end))
                    ,   state   (std::move (v.state))
                {
                }

                template<typename TRangeBuilder>
                CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
                {
                    return range_builder.build (*this);
                }

                CPPLINQ_INLINEMETHOD return_type front () const CPPLINQ_NOEXCEPT
                {
                    assert (state == state_iterating);
                    assert (iter < end);

                    return (*values)[iter];
                }

                CPPLINQ_INLINEMETHOD bool next () CPPLINQ_NOEXCEPT
                {
                    switch (state)
                    {
                    case state_initial:
                        {
                            auto has_elements = iter < end;
                            state = has_elements ? state_iterating : state_end;
                            return has_elements;
                        }
                        break;
                    case state_iterating:
                        {
                            ++iter;

                            auto has_elements = iter < end;
                            state = has_elements ? state_iterating : state_end;
                            return has_elements;
                        }
                        break;
                    case state_end:
                    default:
                        return false;
                    }
                }

            };

            CPPLINQ_METHOD lookup_range operator[](key_type const & key) const CPPLINQ_NOEXCEPT
            {
                if (values.empty ())
                {
                    return lookup_range (std::addressof (values), 0U, 0U);
                }

                if (keys.empty ())
                {
                    return lookup_range (std::addressof (values), 0U, 0U);
                }

                auto find = std::lower_bound (
                        keys.begin ()
                    ,   keys.end  ()
                    ,   typename keys_type::value_type (key, 0U)
                    ,   [](typename keys_type::value_type const & l, typename keys_type::value_type const & r)
                        {
                            return l.first < r.first;
                        });

                if (find == keys.end ())
                {
                    return lookup_range (std::addressof (values), 0U, 0U);
                }

                auto next = find + 1;
                if (next == keys.end ())
                {
                    return lookup_range (std::addressof (values), find->second, values.size ());
                }

                return lookup_range (std::addressof (values), find->second, next->second);
            }

            CPPLINQ_INLINEMETHOD size_type size_of_keys () const CPPLINQ_NOEXCEPT
            {
                return keys.size ();
            }

            CPPLINQ_INLINEMETHOD size_type size_of_values () const CPPLINQ_NOEXCEPT
            {
                return values.size ();
            }

            CPPLINQ_INLINEMETHOD from_range<values_iterator_type> range_of_values () const CPPLINQ_NOEXCEPT
            {
                return from_range<values_iterator_type> (
                        values.begin ()
                    ,   values.end ()
                    );
            }

        private:
            values_type values  ;
            keys_type   keys    ;
        };

        template<typename TKeyPredicate>
        struct to_lookup_builder : base_builder
        {
            static TKeyPredicate get_key_predicate ();

            typedef                     to_lookup_builder<TKeyPredicate>    this_type           ;
            typedef                     TKeyPredicate                       key_predicate_type  ;

            key_predicate_type          key_predicate   ;

            CPPLINQ_INLINEMETHOD explicit to_lookup_builder (key_predicate_type key_predicate) CPPLINQ_NOEXCEPT
                :   key_predicate   (std::move (key_predicate))
            {
            }

            CPPLINQ_INLINEMETHOD to_lookup_builder (to_lookup_builder const & v)
                :   key_predicate (v.key_predicate)
            {
            }

            CPPLINQ_INLINEMETHOD to_lookup_builder (to_lookup_builder && v) CPPLINQ_NOEXCEPT
                :   key_predicate (std::move (v.key_predicate))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD lookup<
                    typename get_transformed_type<key_predicate_type, typename TRange::value_type>::type
                ,   typename TRange::value_type
                > build (TRange range) const
            {
                typedef lookup<
                    typename get_transformed_type<key_predicate_type, typename TRange::value_type>::type
                ,   typename TRange::value_type
                >   result_type;

                result_type result (16U, range, key_predicate);

                return result;
            }

        };

        // -------------------------------------------------------------------------

        template<typename TPredicate>
        struct for_each_builder : base_builder
        {
            typedef                 for_each_builder<TPredicate>    this_type       ;
            typedef                 TPredicate                      predicate_type  ;

            predicate_type          predicate;

            CPPLINQ_INLINEMETHOD explicit for_each_builder (predicate_type predicate) CPPLINQ_NOEXCEPT
                :   predicate    (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD for_each_builder (for_each_builder const & v) CPPLINQ_NOEXCEPT
                :   predicate (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD for_each_builder (for_each_builder && v) CPPLINQ_NOEXCEPT
                :   predicate (std::move (v.predicate))
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD void build (TRange range) const
            {
                while (range.next ())
                {
                    predicate (range.front ());
                }
            }

        };

        // -------------------------------------------------------------------------

        template<typename TPredicate>
        struct first_predicate_builder : base_builder
        {
            typedef                 first_predicate_builder<TPredicate>     this_type           ;
            typedef                 TPredicate                              predicate_type      ;

            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD first_predicate_builder (predicate_type predicate) CPPLINQ_NOEXCEPT
                :   predicate (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD first_predicate_builder (first_predicate_builder const & v) CPPLINQ_NOEXCEPT
                :   predicate (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD first_predicate_builder (first_predicate_builder && v) CPPLINQ_NOEXCEPT
                : predicate (std::move (v.predicate))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::return_type build (TRange range)
            {
                while (range.next ())
                {
                    if (predicate (range.front ()))
                    {
                        return range.front ();
                    }
                }

                throw sequence_empty_exception ();
            }

        };

        // -------------------------------------------------------------------------

        struct first_builder : base_builder
        {
            typedef                 first_builder                   this_type       ;

            CPPLINQ_INLINEMETHOD first_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD first_builder (first_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD first_builder (first_builder && v) CPPLINQ_NOEXCEPT
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::return_type build (TRange range)
            {
                if (range.next ())
                {
                    return range.front ();
                }

                throw sequence_empty_exception ();
            }

        };

        // -------------------------------------------------------------------------

        template<typename TPredicate>
        struct first_or_default_predicate_builder : base_builder
        {
            typedef                 first_or_default_predicate_builder<TPredicate>  this_type       ;
            typedef                 TPredicate                                      predicate_type  ;

            predicate_type          predicate;

            CPPLINQ_INLINEMETHOD first_or_default_predicate_builder (predicate_type predicate) CPPLINQ_NOEXCEPT
                :   predicate (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD first_or_default_predicate_builder (first_or_default_predicate_builder const & v) CPPLINQ_NOEXCEPT
                :   predicate (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD first_or_default_predicate_builder (first_or_default_predicate_builder && v) CPPLINQ_NOEXCEPT
                : predicate (std::move (v.predicate))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                while (range.next ())
                {
                    if (predicate (range.front ()))
                    {
                        return range.front ();
                    }
                }

                return typename TRange::value_type ();
            }

        };


        struct first_or_default_builder : base_builder
        {
            typedef                 first_or_default_builder                   this_type       ;

            CPPLINQ_INLINEMETHOD first_or_default_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD first_or_default_builder (first_or_default_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD first_or_default_builder (first_or_default_builder && v) CPPLINQ_NOEXCEPT
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                if (range.next ())
                {
                    return range.front ();
                }

                return typename TRange::value_type ();
            }

        };

        // -------------------------------------------------------------------------
        template<typename TPredicate>
        struct last_or_default_predicate_builder : base_builder
        {
            typedef                 last_or_default_predicate_builder<TPredicate>   this_type       ;
            typedef                 TPredicate                                      predicate_type  ;

            predicate_type          predicate;

            CPPLINQ_INLINEMETHOD last_or_default_predicate_builder (predicate_type predicate) CPPLINQ_NOEXCEPT
                :   predicate (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD last_or_default_predicate_builder (last_or_default_predicate_builder const & v) CPPLINQ_NOEXCEPT
                :   predicate (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD last_or_default_predicate_builder (last_or_default_predicate_builder && v) CPPLINQ_NOEXCEPT
                :   predicate (std::move (v.predicate))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                auto current = typename TRange::value_type ();

                while (range.next ())
                {
                    if (predicate (range.front ()))
                    {
                        current = std::move (range.front ());
                    }
                }

                return current;
            }

        };

        struct last_or_default_builder : base_builder
        {
            typedef                 last_or_default_builder                   this_type       ;

            CPPLINQ_INLINEMETHOD last_or_default_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD last_or_default_builder (last_or_default_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD last_or_default_builder (last_or_default_builder && v) CPPLINQ_NOEXCEPT
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                auto current = typename TRange::value_type ();

                while (range.next ())
                {
                    current = std::move (range.front ());
                }

                return current;
            }

        };

        // -------------------------------------------------------------------------

        template <typename TPredicate>
        struct count_predicate_builder : base_builder
        {
            typedef                 count_predicate_builder<TPredicate>     this_type       ;
            typedef                 TPredicate                              predicate_type  ;

            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD count_predicate_builder (predicate_type  predicate) CPPLINQ_NOEXCEPT
                :   predicate   (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD count_predicate_builder (count_predicate_builder const & v) CPPLINQ_NOEXCEPT
                :   predicate   (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD count_predicate_builder (count_predicate_builder && v) CPPLINQ_NOEXCEPT
                :   predicate   (std::move (v.predicate))
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD size_type build (TRange range) const
            {
                size_type count = 0U;
                while (range.next ())
                {
                    if (predicate (range.front ()))
                    {
                        ++count;
                    }
                }
                return count;
            }

        };

        struct count_builder : base_builder
        {
            typedef                 count_builder                   this_type       ;

            CPPLINQ_INLINEMETHOD count_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD count_builder (count_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD count_builder (count_builder && v) CPPLINQ_NOEXCEPT
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD size_type build (TRange range) const
            {
                size_type count = 0U;
                while (range.next ())
                {
                    ++count;
                }
                return count;
            }

        };

        // -------------------------------------------------------------------------

        template <typename TSelector>
        struct sum_selector_builder : base_builder
        {
            typedef                 sum_selector_builder<TSelector>     this_type       ;
            typedef                 TSelector                           selector_type   ;

            selector_type           selector;

            CPPLINQ_INLINEMETHOD sum_selector_builder (selector_type selector) CPPLINQ_NOEXCEPT
                :   selector (std::move (selector))
            {
            }

            CPPLINQ_INLINEMETHOD sum_selector_builder (sum_selector_builder const & v) CPPLINQ_NOEXCEPT
                :   selector (v.selector)
            {
            }

            CPPLINQ_INLINEMETHOD sum_selector_builder (sum_selector_builder && v) CPPLINQ_NOEXCEPT
                :   selector (std::move (v.selector))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                auto sum = typename TRange::value_type ();
                while (range.next ())
                {
                    sum += selector (range.front ());
                }
                return sum;
            }

        };

        struct sum_builder : base_builder
        {
            typedef                 sum_builder                     this_type       ;

            CPPLINQ_INLINEMETHOD sum_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD sum_builder (sum_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD sum_builder (sum_builder && v) CPPLINQ_NOEXCEPT
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                auto sum = typename TRange::value_type ();
                while (range.next ())
                {
                    sum += range.front ();
                }
                return sum;
            }

        };

        // -------------------------------------------------------------------------

        template <typename TSelector>
        struct max_selector_builder : base_builder
        {
            typedef                 max_selector_builder<TSelector>     this_type       ;
            typedef                 TSelector                           selector_type   ;

            selector_type           selector;

            CPPLINQ_INLINEMETHOD max_selector_builder (selector_type selector) CPPLINQ_NOEXCEPT
                :   selector (std::move (selector))
            {
            }

            CPPLINQ_INLINEMETHOD max_selector_builder (max_selector_builder const & v) CPPLINQ_NOEXCEPT
                :   selector (v.selector)
            {
            }

            CPPLINQ_INLINEMETHOD max_selector_builder (max_selector_builder && v) CPPLINQ_NOEXCEPT
                :   selector (std::move (v.selector))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                auto current = std::numeric_limits<typename TRange::value_type>::min ();
                while (range.next ())
                {
                    auto v = selector (range.front ());
                    if (current < v)
                    {
                        current = std::move (v);
                    }
                }

                return current;
            }

        };

        struct max_builder : base_builder
        {
            typedef                 max_builder                         this_type       ;

            CPPLINQ_INLINEMETHOD max_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD max_builder (max_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD max_builder (max_builder && v) CPPLINQ_NOEXCEPT
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                auto current = std::numeric_limits<typename TRange::value_type>::min ();
                while (range.next ())
                {
                    auto v = range.front ();
                    if (current < v)
                    {
                        current = std::move (v);
                    }
                }

                return current;
            }

        };

        // -------------------------------------------------------------------------

        template <typename TSelector>
        struct min_selector_builder : base_builder
        {
            typedef                 min_selector_builder<TSelector>     this_type       ;
            typedef                 TSelector                           selector_type   ;

            selector_type           selector;

            CPPLINQ_INLINEMETHOD min_selector_builder (selector_type selector) CPPLINQ_NOEXCEPT
                :   selector (std::move (selector))
            {
            }

            CPPLINQ_INLINEMETHOD min_selector_builder (min_selector_builder const & v) CPPLINQ_NOEXCEPT
                :   selector (v.selector)
            {
            }

            CPPLINQ_INLINEMETHOD min_selector_builder (min_selector_builder && v) CPPLINQ_NOEXCEPT
                :   selector (std::move (v.selector))
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                auto current = std::numeric_limits<typename TRange::value_type>::max ();
                while (range.next ())
                {
                    auto v = selector (range.front ());
                    if (v < current)
                    {
                        current = std::move (v);
                    }
                }

                return current;
            }

        };

        struct min_builder : base_builder
        {
            typedef                 min_builder                         this_type       ;

            CPPLINQ_INLINEMETHOD min_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD min_builder (min_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD min_builder (min_builder && v) CPPLINQ_NOEXCEPT
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                auto current = std::numeric_limits<typename TRange::value_type>::max ();
                while (range.next ())
                {
                    auto v = range.front ();
                    if (v < current)
                    {
                        current = std::move (v);
                    }
                }

                return current;
            }

        };

        // -------------------------------------------------------------------------

        template <typename TSelector>
        struct avg_selector_builder : base_builder
        {
            typedef                 avg_selector_builder<TSelector>     this_type       ;
            typedef                 TSelector                           selector_type   ;

            selector_type           selector;

            CPPLINQ_INLINEMETHOD avg_selector_builder (selector_type selector) CPPLINQ_NOEXCEPT
                :   selector (std::move (selector))
            {
            }

            CPPLINQ_INLINEMETHOD avg_selector_builder (avg_selector_builder const & v) CPPLINQ_NOEXCEPT
                :   selector (v.selector)
            {
            }

            CPPLINQ_INLINEMETHOD avg_selector_builder (avg_selector_builder && v) CPPLINQ_NOEXCEPT
                :   selector (std::move (v.selector))
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                auto sum = typename TRange::value_type ();
                int count = 0;
                while (range.next ())
                {
                    sum += selector (range.front ());
                    ++count;
                }

                if (count == 0)
                {
                    return sum;
                }

                return sum/count;
            }

        };

        struct avg_builder : base_builder
        {
            typedef                 avg_builder                         this_type       ;

            CPPLINQ_INLINEMETHOD avg_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD avg_builder (avg_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD avg_builder (avg_builder && v) CPPLINQ_NOEXCEPT
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                auto sum = typename TRange::value_type ();
                int  count = 0;
                while (range.next ())
                {
                    sum += range.front ();
                    ++count;
                }

                if (count == 0)
                {
                    return sum;
                }

                return sum/count;
            }

        };

        // -------------------------------------------------------------------------

        template <typename TAccumulate, typename TAccumulator>
        struct aggregate_builder : base_builder
        {
            typedef                 aggregate_builder<TAccumulate, TAccumulator>    this_type       ;
            typedef                 TAccumulator                                    accumulator_type;
            typedef                 TAccumulate                                     seed_type;

            seed_type               seed;
            accumulator_type        accumulator;

            CPPLINQ_INLINEMETHOD aggregate_builder (seed_type seed, accumulator_type accumulator) CPPLINQ_NOEXCEPT
                :   seed        (std::move (seed))
                ,   accumulator (std::move (accumulator))
            {
            }

            CPPLINQ_INLINEMETHOD aggregate_builder (aggregate_builder const & v) CPPLINQ_NOEXCEPT
                :   seed        (v.seed)
                ,   accumulator (v.accumulator)
            {
            }

            CPPLINQ_INLINEMETHOD aggregate_builder (aggregate_builder && v) CPPLINQ_NOEXCEPT
                :   seed        (std::move (v.seed))
                ,   accumulator (std::move (v.accumulator))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD seed_type build (TRange range) const
            {
                auto sum = seed;
                while (range.next ())
                {
                    sum = accumulator (sum, range.front ());
                }
                return sum;
            }

        };

        template <typename TAccumulate, typename TAccumulator, typename TSelector>
        struct aggregate_result_selector_builder : base_builder
        {
            typedef                 aggregate_result_selector_builder<TAccumulate, TAccumulator, TSelector> this_type       ;
            typedef                 TAccumulator                                                            accumulator_type;
            typedef                 TAccumulate                                                             seed_type;
            typedef                 TSelector                                                               result_selector_type;

            seed_type               seed;
            accumulator_type        accumulator;
            result_selector_type    result_selector;

            CPPLINQ_INLINEMETHOD aggregate_result_selector_builder (seed_type seed, accumulator_type accumulator, result_selector_type result_selector) CPPLINQ_NOEXCEPT
                :   seed            (std::move (seed))
                ,   accumulator     (std::move (accumulator))
                ,   result_selector (std::move (result_selector))
            {
            }

            CPPLINQ_INLINEMETHOD aggregate_result_selector_builder (aggregate_result_selector_builder const & v) CPPLINQ_NOEXCEPT
                :   seed            (v.seed)
                ,   accumulator     (v.accumulator)
                ,   result_selector (v.result_selector)
            {
            }

            CPPLINQ_INLINEMETHOD aggregate_result_selector_builder (aggregate_result_selector_builder && v) CPPLINQ_NOEXCEPT
                :   seed            (std::move (v.seed))
                ,   accumulator     (std::move (v.accumulator))
                ,   result_selector (std::move (v.result_selector))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD auto build (TRange range) const -> decltype (result_selector (seed))
            {
                auto sum = seed;
                while (range.next ())
                {
                    sum = accumulator (sum, range.front ());
                }

                return result_selector (sum);
            }

        };

        // -------------------------------------------------------------------------

        template <typename TOtherRange, typename TComparer>
        struct sequence_equal_predicate_builder : base_builder
        {
            typedef                 sequence_equal_predicate_builder<TOtherRange,TComparer>     this_type       ;
            typedef                 TOtherRange                                                 other_range_type;
            typedef                 TComparer                                                   comparer_type   ;

            other_range_type        other_range         ;
            comparer_type           comparer            ;

            CPPLINQ_INLINEMETHOD sequence_equal_predicate_builder (
                    TOtherRange     other_range
                ,   comparer_type   comparer) CPPLINQ_NOEXCEPT
                :   other_range (std::move (other_range))
                ,   comparer    (std::move (comparer))
            {
            }

            CPPLINQ_INLINEMETHOD sequence_equal_predicate_builder (sequence_equal_predicate_builder const & v) CPPLINQ_NOEXCEPT
                :   other_range (v.other_range)
                ,   comparer    (v.comparer)
            {
            }

            CPPLINQ_INLINEMETHOD sequence_equal_predicate_builder (sequence_equal_predicate_builder && v) CPPLINQ_NOEXCEPT
                :   other_range (std::move (v.other_range))
                ,   comparer    (std::move (v.comparer))
            {
            }

            template <typename TRange>
            CPPLINQ_INLINEMETHOD bool build (TRange range) const
            {
                auto copy = other_range;
                for (;;)
                {
                    bool next1 = range.next ();
                    bool next2 = copy.next ();

                    // sequences are not of same length
                    if (next1 != next2)
                    {
                        return false;
                    }

                    // both sequences are over, next1 = next2 = false
                    if (!next1)
                    {
                        return true;
                    }

                    if (!comparer (range.front (), copy.front ()))
                    {
                        return false;
                    }
                }
            }
        };

        template <typename TOtherRange>
        struct sequence_equal_builder : base_builder
        {
            typedef                 sequence_equal_builder<TOtherRange>     this_type       ;
            typedef                 TOtherRange                             other_range_type;

            other_range_type        other_range         ;

            CPPLINQ_INLINEMETHOD sequence_equal_builder (TOtherRange other_range) CPPLINQ_NOEXCEPT
                : other_range (std::move (other_range))
            {
            }

            CPPLINQ_INLINEMETHOD sequence_equal_builder (sequence_equal_builder const & v) CPPLINQ_NOEXCEPT
                : other_range (v.other_range)
            {
            }

            CPPLINQ_INLINEMETHOD sequence_equal_builder (sequence_equal_builder && v) CPPLINQ_NOEXCEPT
                : other_range (std::move (v.other_range))
            {
            }

            template <typename TRange>
            CPPLINQ_INLINEMETHOD bool build (TRange range) const
            {
                auto copy = other_range;
                for (;;)
                {
                    bool next1 = range.next ();
                    bool next2 = copy.next ();

                    // sequences are not of same length
                    if (next1 != next2)
                    {
                        return false;
                    }

                    // both sequences are over, next1 = next2 = false
                    if (!next1)
                    {
                        return true;
                    }

                    if (range.front () != copy.front ())
                    {
                        return false;
                    }
                }
            }
        };

        // -------------------------------------------------------------------------

        template<typename TCharType>
        struct concatenate_builder : base_builder
        {
            typedef                         concatenate_builder<TCharType>  this_type       ;

            std::basic_string<TCharType>    separator   ;
            size_type                       capacity    ;

            CPPLINQ_INLINEMETHOD concatenate_builder (
                    std::basic_string<TCharType>    separator
                ,   size_type capacity
                ) CPPLINQ_NOEXCEPT
                :   separator   (std::move (separator))
                ,   capacity    (capacity)
            {
            }

            CPPLINQ_INLINEMETHOD concatenate_builder (concatenate_builder const & v) CPPLINQ_NOEXCEPT
                :   separator   (v.separator)
                ,   capacity    (v.capacity)
            {
            }

            CPPLINQ_INLINEMETHOD concatenate_builder (concatenate_builder && v) CPPLINQ_NOEXCEPT
                :   separator   (std::move (v.separator))
                ,   capacity    (std::move (v.capacity))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename std::basic_string<TCharType> build (TRange range) const
            {
                auto                    first  =   true ;
                std::vector<TCharType>  buffer          ;

                buffer.reserve (capacity);


                while (range.next ())
                {
                    if (first)
                    {
                        first = false;
                    }
                    else
                    {
                        buffer.insert (
                                buffer.end ()
                            ,   separator.begin ()
                            ,   separator.end ()
                            );
                    }

                    auto v = range.front ();

                    buffer.insert (
                            buffer.end ()
                        ,   v.begin ()
                        ,   v.end ()
                        );
                }

                return std::basic_string<TCharType> (
                        buffer.begin ()
                    ,   buffer.end ()
                    );
            }

        };

        // -------------------------------------------------------------------------
        template <typename TPredicate>
        struct any_predicate_builder : base_builder
        {
            typedef                 any_predicate_builder<TPredicate>   this_type       ;
            typedef                 TPredicate                          predicate_type  ;

            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD any_predicate_builder (predicate_type  predicate) CPPLINQ_NOEXCEPT
                :   predicate   (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD any_predicate_builder (any_predicate_builder const & v) CPPLINQ_NOEXCEPT
                :   predicate   (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD any_predicate_builder (any_predicate_builder && v) CPPLINQ_NOEXCEPT
                :   predicate   (std::move (v.predicate))
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD bool build (TRange range) const
            {
                bool any = false;
                while (range.next () && !any)
                {
                    any = predicate (range.front ());
                }
                return any;
            }
        };

        struct any_builder : base_builder
        {
            typedef                 any_builder                   this_type       ;

            CPPLINQ_INLINEMETHOD any_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD any_builder (any_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD any_builder (any_builder && v) CPPLINQ_NOEXCEPT
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD bool build (TRange range) const
            {
                return range.next ();
            }

        };

        // -------------------------------------------------------------------------

        template <typename TPredicate>
        struct all_predicate_builder : base_builder
        {
            typedef                 all_predicate_builder<TPredicate>   this_type       ;
            typedef                 TPredicate                          predicate_type  ;

            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD all_predicate_builder (predicate_type  predicate) CPPLINQ_NOEXCEPT
                :   predicate   (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD all_predicate_builder (all_predicate_builder const & v) CPPLINQ_NOEXCEPT
                :   predicate   (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD all_predicate_builder (all_predicate_builder && v) CPPLINQ_NOEXCEPT
                :   predicate   (std::move (v.predicate))
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD bool build (TRange range) const
            {
                while (range.next ())
                {
                    if (!predicate (range.front ()))
                    {
                        return false;
                    }
                }

                return true;
            }
        };

        // -------------------------------------------------------------------------

        template <typename TValue>
        struct contains_builder : base_builder
        {
            typedef                 contains_builder<TValue>        this_type       ;
            typedef                 TValue                          value_type      ;

            value_type              value;

            CPPLINQ_INLINEMETHOD contains_builder (value_type value) CPPLINQ_NOEXCEPT
                :   value (std::move (value))
            {
            }

            CPPLINQ_INLINEMETHOD contains_builder (contains_builder const & v) CPPLINQ_NOEXCEPT
                :   value (v.value)
            {
            }

            CPPLINQ_INLINEMETHOD contains_builder (contains_builder && v) CPPLINQ_NOEXCEPT
                :   value (std::move (v.value))
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD bool build (TRange range) const
            {
                while (range.next ())
                {
                    if (range.front () == value)
                    {
                        return true;
                    }
                }

                return false;
            }

        };

        template <typename TValue, typename TPredicate>
        struct contains_predicate_builder : base_builder
        {
            typedef                 contains_predicate_builder<TValue, TPredicate>  this_type       ;
            typedef                 TValue                                          value_type      ;
            typedef                 TPredicate                                      predicate_type  ;

            value_type              value;
            predicate_type          predicate   ;

            CPPLINQ_INLINEMETHOD contains_predicate_builder (value_type value, predicate_type predicate) CPPLINQ_NOEXCEPT
                :   value       (std::move (value))
                ,   predicate   (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD contains_predicate_builder (contains_predicate_builder const & v) CPPLINQ_NOEXCEPT
                :   value       (v.value)
                ,   predicate   (v.predicate)
            {
            }

            CPPLINQ_INLINEMETHOD contains_predicate_builder (contains_predicate_builder && v) CPPLINQ_NOEXCEPT
                :   value       (std::move (v.value))
                ,   predicate   (std::move (v.predicate))
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD bool build (TRange range) const
            {
                while (range.next ())
                {
                    if (predicate (range.front (), value))
                    {
                        return true;
                    }
                }

                return false;
            }

        };

        // -------------------------------------------------------------------------

        struct element_at_or_default_builder : base_builder
        {
            typedef                 element_at_or_default_builder   this_type       ;

            size_type               index;

            CPPLINQ_INLINEMETHOD element_at_or_default_builder (size_type index) CPPLINQ_NOEXCEPT
                :   index (std::move (index))
            {
            }

            CPPLINQ_INLINEMETHOD element_at_or_default_builder (element_at_or_default_builder const & v) CPPLINQ_NOEXCEPT
                :   index (v.index)
            {
            }

            CPPLINQ_INLINEMETHOD element_at_or_default_builder (element_at_or_default_builder && v) CPPLINQ_NOEXCEPT
                :   index (std::move (v.index))
            {
            }


            template<typename TRange>
            CPPLINQ_INLINEMETHOD typename TRange::value_type build (TRange range) const
            {
                size_type current = 0U;

                while (range.next ())
                {
                    if (current < index)
                    {
                        ++current;
                    }
                    else
                    {
                        return range.front ();
                    }
                }

                return typename TRange::value_type ();

            }

        };

        // -------------------------------------------------------------------------

        template<typename TRange>
        struct pairwise_range : base_range
        {
            typedef                 pairwise_range<TRange>                      this_type           ;
            typedef                 TRange                                      range_type          ;

            typedef                 typename TRange::value_type                 element_type        ;
            typedef                 std::pair<element_type,element_type>        value_type          ;
            typedef                 value_type                                  return_type         ;

            enum
            {
                returns_reference   = 0     ,
            };


            range_type                   range               ;
            opt<element_type>            previous            ;
            opt<element_type>            current             ;

            CPPLINQ_INLINEMETHOD pairwise_range (
                    range_type          range
                ) CPPLINQ_NOEXCEPT
                :   range               (std::move (range))
            {
            }

            CPPLINQ_INLINEMETHOD pairwise_range (pairwise_range const & v) CPPLINQ_NOEXCEPT
                :   range               (v.range)
                ,   previous            (v.previous)
                ,   current             (v.current)
            {
            }

            CPPLINQ_INLINEMETHOD pairwise_range (pairwise_range && v) CPPLINQ_NOEXCEPT
                :   range               (std::move (v.range))
                ,   previous            (std::move (v.previous))
                ,   current             (std::move (v.current))
            {
            }

            template<typename TPairwiseBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TPairwiseBuilder, this_type>::type operator>>(TPairwiseBuilder pairwise_builder) const
            {
                return pairwise_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                assert (previous.has_value ());
                assert (current.has_value ());
                return std::make_pair (previous.get (), current.get ());
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                if (!previous.has_value ())
                {
                    if (range.next ())
                    {
                        current = range.front ();
                    }
                    else
                    {
                        return false;
                    }
                }

                previous.swap (current);

                if (range.next ())
                {
                    current = range.front ();
                    return true;
                }

                return false;
            }
        };

        struct pairwise_builder : base_builder
        {
            typedef             pairwise_builder     this_type   ;

            CPPLINQ_INLINEMETHOD pairwise_builder () CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD pairwise_builder (pairwise_builder const & v) CPPLINQ_NOEXCEPT
            {
            }

            CPPLINQ_INLINEMETHOD pairwise_builder (pairwise_builder && v) CPPLINQ_NOEXCEPT
            {
            }

            template<typename TRange>
            CPPLINQ_INLINEMETHOD pairwise_range<TRange> build (TRange range) const
            {
                return pairwise_range<TRange> (std::move (range));
            }
        };

        // -------------------------------------------------------------------------

        template<typename TRange, typename TOtherRange>
        struct zip_with_range : base_range
        {
            typedef             zip_with_range<TRange, TOtherRange>                     this_type          ;
            typedef             TRange                                                  range_type         ;
            typedef             TOtherRange                                             other_range_type   ;

            typedef    typename cleanup_type<typename TRange::value_type>::type         left_element_type  ;
            typedef    typename cleanup_type<typename TOtherRange::value_type>::type    right_element_type ;
            typedef             std::pair<left_element_type,right_element_type>         value_type         ;
            typedef             value_type                                              return_type        ;
            enum
            {
                returns_reference   = 0 ,
            };

            range_type                  range               ;
            other_range_type            other_range         ;

            CPPLINQ_INLINEMETHOD zip_with_range (
                        range_type          range
                    ,   other_range_type    other_range
                ) CPPLINQ_NOEXCEPT
                :   range               (std::move (range))
                ,   other_range         (std::move (other_range))
            {
            }

            CPPLINQ_INLINEMETHOD zip_with_range (zip_with_range const & v) CPPLINQ_NOEXCEPT
                :   range               (v.range)
                ,   other_range         (v.other_range)
            {
            }

            CPPLINQ_INLINEMETHOD zip_with_range (zip_with_range && v) CPPLINQ_NOEXCEPT
                :   range               (std::move (v.range))
                ,   other_range         (std::move (v.other_range))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                return std::make_pair (range.front (), other_range.front ());
            }

            CPPLINQ_INLINEMETHOD bool next ()
            {
                return range.next () && other_range.next ();
            }
        };

        template <typename TOtherRange>
        struct zip_with_builder : base_builder
        {
            typedef                 zip_with_builder<TOtherRange>         this_type       ;
            typedef                 TOtherRange                           other_range_type;

            other_range_type        other_range         ;

            CPPLINQ_INLINEMETHOD zip_with_builder (TOtherRange other_range) CPPLINQ_NOEXCEPT
                : other_range (std::move (other_range))
            {
            }

            CPPLINQ_INLINEMETHOD zip_with_builder (zip_with_builder const & v) CPPLINQ_NOEXCEPT
                : other_range (v.other_range)
            {
            }

            CPPLINQ_INLINEMETHOD zip_with_builder (zip_with_builder && v) CPPLINQ_NOEXCEPT
                : other_range (std::move (v.other_range))
            {
            }

            template <typename TRange>
            CPPLINQ_INLINEMETHOD zip_with_range<TRange, TOtherRange> build (TRange range) const
            {
                return zip_with_range<TRange, TOtherRange> (std::move (range), std::move (other_range));
            }
        };

        // -------------------------------------------------------------------------

        template<typename TPredicate>
        struct generate_range : base_range
        {
            static         TPredicate get_predicate ();

            typedef        decltype (get_predicate ()())                    raw_opt_value_type  ;
            typedef        typename cleanup_type<raw_opt_value_type>::type  opt_value_type      ;

            typedef        decltype (*(get_predicate ()()))                 raw_value_type      ;
            typedef        typename cleanup_type<raw_value_type>::type      value_type          ;

            typedef                 generate_range<TPredicate>      this_type           ;
            typedef                 TPredicate                      predicate_type      ;
            typedef                 value_type const &              return_type         ;

            enum
            {
                returns_reference = 1,
            };

            TPredicate              predicate       ;
            opt_value_type          current_value   ;

            CPPLINQ_INLINEMETHOD generate_range (
                    TPredicate predicate
                ) CPPLINQ_NOEXCEPT
                :   predicate   (std::move (predicate))
            {
            }

            CPPLINQ_INLINEMETHOD generate_range (generate_range const & v) CPPLINQ_NOEXCEPT
                :   predicate       (v.predicate)
                ,   current_value   (v.current_value)
            {
            }

            CPPLINQ_INLINEMETHOD generate_range (generate_range && v) CPPLINQ_NOEXCEPT
                :   predicate       (std::move (v.predicate))
                ,   current_value   (std::move (v.current_value))
            {
            }

            template<typename TRangeBuilder>
            CPPLINQ_INLINEMETHOD typename get_builtup_type<TRangeBuilder, this_type>::type operator>>(TRangeBuilder range_builder) const
            {
                return range_builder.build (*this);
            }

            CPPLINQ_INLINEMETHOD return_type front () const
            {
                assert (current_value);
                return *current_value;
            }

            CPPLINQ_INLINEMETHOD bool next () CPPLINQ_NOEXCEPT
            {
                current_value = predicate ();
                return current_value;
            }
        };

        // -------------------------------------------------------------------------

    }   // namespace detail

    // -------------------------------------------------------------------------
    // The interface of cpplinq
    // -------------------------------------------------------------------------

    // Range sources

    template<typename TValueIterator>
    CPPLINQ_INLINEMETHOD detail::from_range<TValueIterator> from_iterators (
            TValueIterator  begin
        ,   TValueIterator  end
        ) CPPLINQ_NOEXCEPT
    {
        return detail::from_range<TValueIterator> (std::move (begin), std::move (end));
    }

    template<typename TContainer>
    CPPLINQ_INLINEMETHOD detail::from_range<typename TContainer::const_iterator> from (
            TContainer  const & container
        )
    {
        return detail::from_range<typename TContainer::const_iterator> (
                container.begin ()
            ,   container.end ()
            );
    }

    template<typename TValueArray>
    CPPLINQ_INLINEMETHOD detail::from_range<typename detail::get_array_properties<TValueArray>::iterator_type> from_array (
            TValueArray & a
        ) CPPLINQ_NOEXCEPT
    {
        typedef detail::get_array_properties<TValueArray>   array_properties;
        typedef typename array_properties::iterator_type iterator_type;

        iterator_type begin = a;
        iterator_type end   = begin + array_properties::size;

        return detail::from_range<typename array_properties::iterator_type> (
                std::move (begin)
            ,   std::move (end)
            );
    }

    template<typename TContainer>
    CPPLINQ_INLINEMETHOD detail::from_copy_range<typename detail::cleanup_type<TContainer>::type> from_copy (
            TContainer&& container
        )
    {
        typedef typename detail::cleanup_type<TContainer>::type container_type;

        return detail::from_copy_range<container_type> (
                std::forward<TContainer> (container)
            );
    }

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::generate_range<TPredicate> generate (
        TPredicate predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::generate_range<TPredicate> (std::move (predicate));
    }

    // Restriction operators

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::where_builder<TPredicate> where (
            TPredicate      predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::where_builder<TPredicate> (std::move (predicate));
    }

    // Projection operators

    CPPLINQ_INLINEMETHOD detail::ref_builder ref () CPPLINQ_NOEXCEPT
    {
        return detail::ref_builder ();
    }

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::select_builder<TPredicate> select (
            TPredicate      predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::select_builder<TPredicate> (std::move (predicate));
    }

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::select_many_builder<TPredicate> select_many (
            TPredicate      predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::select_many_builder<TPredicate> (std::move (predicate));
    }

    template<
                typename TOtherRange
            ,   typename TKeySelector
            ,   typename TOtherKeySelector
            ,   typename TCombiner
            >
    CPPLINQ_INLINEMETHOD detail::join_builder<
                TOtherRange
            ,   TKeySelector
            ,   TOtherKeySelector
            ,   TCombiner
            > join (
                TOtherRange         other_range
            ,   TKeySelector        key_selector
            ,   TOtherKeySelector   other_key_selector
            ,   TCombiner           combiner
        ) CPPLINQ_NOEXCEPT
    {
        return detail::join_builder<
                TOtherRange
            ,   TKeySelector
            ,   TOtherKeySelector
            ,   TCombiner
            >
            (
                std::move (other_range)
            ,   std::move (key_selector)
            ,   std::move (other_key_selector)
            ,   std::move (combiner)
            );
    }

    // Concatenation operators

    template <typename TOtherRange>
    CPPLINQ_INLINEMETHOD detail::concat_builder<TOtherRange> concat (TOtherRange other_range) CPPLINQ_NOEXCEPT
    {
        return detail::concat_builder<TOtherRange> (std::move (other_range));
    }

    // Partitioning operators

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::take_while_builder<TPredicate> take_while (
            TPredicate        predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::take_while_builder<TPredicate> (std::move (predicate));
    }

    CPPLINQ_INLINEMETHOD detail::take_builder take (
            size_type     count
        ) CPPLINQ_NOEXCEPT
    {
        return detail::take_builder (count);
    }

    template <typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::skip_while_builder<TPredicate> skip_while (
            TPredicate        predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::skip_while_builder<TPredicate> (predicate);
    }

    CPPLINQ_INLINEMETHOD detail::skip_builder skip (
            size_type       count
        ) CPPLINQ_NOEXCEPT
    {
        return detail::skip_builder (count);
    }

    // Ordering operators

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::orderby_builder<TPredicate> orderby (
            TPredicate      predicate
        ,   bool            sort_ascending  = true
        ) CPPLINQ_NOEXCEPT
    {
        return detail::orderby_builder<TPredicate> (std::move (predicate), sort_ascending);
    }

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::orderby_builder<TPredicate> orderby_ascending (
            TPredicate      predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::orderby_builder<TPredicate> (std::move (predicate), true);
    }

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::orderby_builder<TPredicate> orderby_descending (
            TPredicate      predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::orderby_builder<TPredicate> (std::move (predicate), false);
    }

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::thenby_builder<TPredicate> thenby (
            TPredicate      predicate
        ,   bool            sort_ascending  = true
        ) CPPLINQ_NOEXCEPT
    {
        return detail::thenby_builder<TPredicate> (std::move (predicate), sort_ascending);
    }

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::thenby_builder<TPredicate> thenby_ascending (
            TPredicate      predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::thenby_builder<TPredicate> (std::move (predicate), true);
    }

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::thenby_builder<TPredicate> thenby_descending (
            TPredicate      predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::thenby_builder<TPredicate> (std::move (predicate), false);
    }

    CPPLINQ_INLINEMETHOD detail::reverse_builder reverse (size_type capacity = 16U) CPPLINQ_NOEXCEPT
    {
        return detail::reverse_builder (capacity);
    }


    // Conversion operators

    namespace experimental
    {
        CPPLINQ_INLINEMETHOD detail::experimental::container_builder container () CPPLINQ_NOEXCEPT
        {
            return detail::experimental::container_builder ();
        }
    }

    template<typename TValue>
    CPPLINQ_INLINEMETHOD detail::opt<typename detail::cleanup_type<TValue>::type> to_opt (TValue && v)
    {
        return detail::opt<typename detail::cleanup_type<TValue>::type> (std::forward<TValue> (v));
    }

    template<typename TValue>
    CPPLINQ_INLINEMETHOD detail::opt<TValue> to_opt ()
    {
        return detail::opt<TValue> ();
    }

    CPPLINQ_INLINEMETHOD detail::to_vector_builder to_vector (size_type capacity = 16U) CPPLINQ_NOEXCEPT
    {
        return detail::to_vector_builder (capacity);
    }

    CPPLINQ_INLINEMETHOD detail::to_list_builder to_list () CPPLINQ_NOEXCEPT
    {
        return detail::to_list_builder ();
    }

    template<typename TKeyPredicate>
    CPPLINQ_INLINEMETHOD detail::to_map_builder<TKeyPredicate> to_map (TKeyPredicate key_predicate) CPPLINQ_NOEXCEPT
    {
        return detail::to_map_builder<TKeyPredicate>(std::move (key_predicate));
    }

    template<typename TKeyPredicate>
    CPPLINQ_INLINEMETHOD detail::to_lookup_builder<TKeyPredicate> to_lookup (TKeyPredicate key_predicate) CPPLINQ_NOEXCEPT
    {
        return detail::to_lookup_builder<TKeyPredicate>(std::move (key_predicate));
    }

    // Equality operators
    template <typename TOtherRange>
    CPPLINQ_INLINEMETHOD detail::sequence_equal_builder<TOtherRange> sequence_equal (TOtherRange other_range) CPPLINQ_NOEXCEPT
    {
        return detail::sequence_equal_builder<TOtherRange> (std::move (other_range));
    }

    template <typename TOtherRange, typename TComparer>
    CPPLINQ_INLINEMETHOD detail::sequence_equal_predicate_builder<TOtherRange, TComparer> sequence_equal (
            TOtherRange other_range
        ,   TComparer   comparer) CPPLINQ_NOEXCEPT
    {
        return detail::sequence_equal_predicate_builder<TOtherRange, TComparer> (std::move (other_range), std::move (comparer));
    }

    // Element operators

    template <typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::first_predicate_builder<TPredicate> first (
            TPredicate predicate
        )
    {
        return detail::first_predicate_builder<TPredicate> (std::move (predicate));
    }

    CPPLINQ_INLINEMETHOD detail::first_builder first ()
    {
        return detail::first_builder ();
    }

    template <typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::first_or_default_predicate_builder<TPredicate> first_or_default (
            TPredicate predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::first_or_default_predicate_builder<TPredicate> (predicate);
    }

    CPPLINQ_INLINEMETHOD detail::first_or_default_builder first_or_default () CPPLINQ_NOEXCEPT
    {
        return detail::first_or_default_builder ();
    }

    template <typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::last_or_default_predicate_builder<TPredicate> last_or_default (
            TPredicate predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::last_or_default_predicate_builder<TPredicate> (predicate);
    }

    CPPLINQ_INLINEMETHOD detail::last_or_default_builder last_or_default () CPPLINQ_NOEXCEPT
    {
        return detail::last_or_default_builder ();
    }

    CPPLINQ_INLINEMETHOD detail::element_at_or_default_builder element_at_or_default (
            size_type   index
        ) CPPLINQ_NOEXCEPT
    {
        return detail::element_at_or_default_builder (index);
    }

    // Generation operators

    CPPLINQ_INLINEMETHOD detail::int_range range (
            int         start
        ,   int         count
        ) CPPLINQ_NOEXCEPT
    {
        auto c      = count > 0 ? count : 0;
        auto end    = (INT_MAX - c) > start ? (start + c) : INT_MAX;
        return detail::int_range (start, end);
    }

    template <typename TValue>
    CPPLINQ_INLINEMETHOD detail::repeat_range<TValue> repeat (
            TValue      element
        ,   int         count
        ) CPPLINQ_NOEXCEPT
    {
        auto c      = count > 0 ? count : 0;
        return detail::repeat_range<TValue> (element, c);
    }

    template <typename TValue>
    CPPLINQ_INLINEMETHOD detail::empty_range<TValue> empty () CPPLINQ_NOEXCEPT
    {
        return detail::empty_range<TValue> ();
    }

    template<typename TValue>
    CPPLINQ_INLINEMETHOD detail::singleton_range<typename detail::cleanup_type<TValue>::type> singleton (TValue&& value) CPPLINQ_NOEXCEPT
    {
        return detail::singleton_range<typename detail::cleanup_type<TValue>::type> (std::forward<TValue> (value));
    }

    // Quantifiers

    template <typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::any_predicate_builder<TPredicate> any (
        TPredicate predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::any_predicate_builder<TPredicate> (std::move (predicate));
    }

    CPPLINQ_INLINEMETHOD detail::any_builder any () CPPLINQ_NOEXCEPT
    {
        return detail::any_builder ();
    }

    template <typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::all_predicate_builder<TPredicate> all (
            TPredicate predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::all_predicate_builder<TPredicate> (std::move (predicate));
    }

    template <typename TValue>
    CPPLINQ_INLINEMETHOD detail::contains_builder<TValue> contains (
            TValue value
        ) CPPLINQ_NOEXCEPT
    {
        return detail::contains_builder<TValue> (value);
    }

    template <typename TValue, typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::contains_predicate_builder<TValue, TPredicate> contains (
            TValue value
        ,   TPredicate predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::contains_predicate_builder<TValue, TPredicate> (value, predicate);
    }

    // Aggregate operators

    template <typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::count_predicate_builder<TPredicate> count (
            TPredicate predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::count_predicate_builder<TPredicate> (std::move (predicate));
    }

    CPPLINQ_INLINEMETHOD detail::count_builder count () CPPLINQ_NOEXCEPT
    {
        return detail::count_builder ();
    }

    template<typename TSelector>
    CPPLINQ_INLINEMETHOD detail::sum_selector_builder<TSelector> sum (
            TSelector selector
        ) CPPLINQ_NOEXCEPT
    {
        return detail::sum_selector_builder<TSelector> (std::move (selector));
    }

    CPPLINQ_INLINEMETHOD detail::sum_builder sum () CPPLINQ_NOEXCEPT
    {
        return detail::sum_builder ();
    }

    /*template<typename TSelector>
    CPPLINQ_INLINEMETHOD detail::max_selector_builder<TSelector> max (TSelector selector) CPPLINQ_NOEXCEPT
    {
        return detail::max_selector_builder<TSelector> (std::move (selector));
    }

    CPPLINQ_INLINEMETHOD detail::max_builder max () CPPLINQ_NOEXCEPT
    {
        return detail::max_builder ();
    }*/

    /*template<typename TSelector>
    CPPLINQ_INLINEMETHOD detail::min_selector_builder<TSelector> min (
            TSelector selector
        ) CPPLINQ_NOEXCEPT
    {
        return detail::min_selector_builder<TSelector> (std::move (selector));
    }

    CPPLINQ_INLINEMETHOD detail::min_builder min () CPPLINQ_NOEXCEPT
    {
        return detail::min_builder ();
    }

    template<typename TSelector>
    CPPLINQ_INLINEMETHOD detail::avg_selector_builder<TSelector> avg (
            TSelector selector
        ) CPPLINQ_NOEXCEPT
    {
        return detail::avg_selector_builder<TSelector> (std::move (selector));
    }

    CPPLINQ_INLINEMETHOD detail::avg_builder avg () CPPLINQ_NOEXCEPT
    {
        return detail::avg_builder ();
    }*/

    template <typename TAccumulate, typename TAccumulator>
    CPPLINQ_INLINEMETHOD detail::aggregate_builder<TAccumulate, TAccumulator> aggregate (
            TAccumulate seed
        ,   TAccumulator accumulator
        ) CPPLINQ_NOEXCEPT
    {
        return detail::aggregate_builder<TAccumulate, TAccumulator> (seed, accumulator);
    }

    template <typename TAccumulate, typename TAccumulator, typename TSelector>
    CPPLINQ_INLINEMETHOD detail::aggregate_result_selector_builder<TAccumulate, TAccumulator, TSelector> aggregate (
            TAccumulate seed
        ,   TAccumulator accumulator
        ,   TSelector result_selector
        ) CPPLINQ_NOEXCEPT
    {
        return detail::aggregate_result_selector_builder<TAccumulate, TAccumulator, TSelector> (seed, accumulator, result_selector);
    }

    // set operators
    CPPLINQ_INLINEMETHOD detail::distinct_builder distinct () CPPLINQ_NOEXCEPT
    {
        return detail::distinct_builder ();
    }

    template <typename TOtherRange>
    CPPLINQ_INLINEMETHOD detail::union_builder<TOtherRange> union_with (TOtherRange other_range) CPPLINQ_NOEXCEPT
    {
        return detail::union_builder<TOtherRange> (std::move (other_range));
    }

    template <typename TOtherRange>
    CPPLINQ_INLINEMETHOD detail::intersect_builder<TOtherRange> intersect_with (TOtherRange other_range) CPPLINQ_NOEXCEPT
    {
        return detail::intersect_builder<TOtherRange> (std::move (other_range));
    }

    template <typename TOtherRange>
    CPPLINQ_INLINEMETHOD detail::except_builder<TOtherRange> except (TOtherRange other_range) CPPLINQ_NOEXCEPT
    {
        return detail::except_builder<TOtherRange> (std::move (other_range));
    }

    // other operators

    template<typename TPredicate>
    CPPLINQ_INLINEMETHOD detail::for_each_builder<TPredicate> for_each (
            TPredicate predicate
        ) CPPLINQ_NOEXCEPT
    {
        return detail::for_each_builder<TPredicate> (std::move (predicate));
    }

    CPPLINQ_INLINEMETHOD detail::concatenate_builder<char> concatenate (
            std::string separator
        ,   size_type capacity = 16U
        ) CPPLINQ_NOEXCEPT
    {
        return detail::concatenate_builder<char> (
                std::move (separator)
            ,   capacity
            );
    }

    CPPLINQ_INLINEMETHOD detail::concatenate_builder<wchar_t> concatenate (
            std::wstring separator
        ,   size_type capacity = 16U
        ) CPPLINQ_NOEXCEPT
    {
        return detail::concatenate_builder<wchar_t> (
                std::move (separator)
            ,   capacity
            );
    }

    CPPLINQ_INLINEMETHOD detail::pairwise_builder pairwise () CPPLINQ_NOEXCEPT
    {
        return detail::pairwise_builder ();
    }

    template <typename TOtherRange>
    CPPLINQ_INLINEMETHOD detail::zip_with_builder<TOtherRange> zip_with (TOtherRange other_range) CPPLINQ_NOEXCEPT
    {
        return detail::zip_with_builder<TOtherRange> (std::move (other_range));
    }

    // -------------------------------------------------------------------------

}
// ----------------------------------------------------------------------------
#ifdef _MSC_VER
#   pragma warning (pop)
#endif
// ----------------------------------------------------------------------------
#endif  // CPPLINQ__HEADER_GUARD
// ----------------------------------------------------------------------------
