/***************************************************************************
 * blitz/tinyvec.cc  Declaration of TinyVector methods
 *
 * $Id$
 *
 * Copyright (C) 1997-2011 Todd Veldhuizen <tveldhui@acm.org>
 *
 * This file is a part of Blitz.
 *
 * Blitz is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * Blitz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with Blitz.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Suggestions:          blitz-devel@lists.sourceforge.net
 * Bugs:                 blitz-support@lists.sourceforge.net    
 *
 * For more information, please see the Blitz++ Home Page:
 *    https://sourceforge.net/projects/blitz/
 *
 ***************************************************************************/

#ifndef BZ_TINYVEC2_CC
#define BZ_TINYVEC2_CC

#include <blitz/tinyvec2.h>
#include <blitz/update.h>
#include <blitz/array/asexpr.h>

BZ_NAMESPACE(blitz)

/*
 * Constructors
 */

template<typename P_numtype, int N_length>
_bz_forceinline
TinyVector<P_numtype, N_length>::TinyVector(const T_numtype initValue) {
    for (int i=0; i < N_length; ++i)
        data_[i] = initValue;
}

template<typename P_numtype, int N_length>
_bz_forceinline 
TinyVector<P_numtype, N_length>::TinyVector(const TinyVector<T_numtype, N_length>& x) {
    for (int i=0; i < N_length; ++i)
        data_[i] = x.data_[i];
}

template<typename P_numtype, int N_length>
template<typename P_numtype2>
_bz_forceinline 
TinyVector<P_numtype, N_length>::TinyVector(const TinyVector<P_numtype2, N_length>& x) {
    for (int i=0; i < N_length; ++i)
        data_[i] = static_cast<P_numtype>(x[i]);
}


/*
 * Assignment-type operators
 */

template<typename P_numtype, int N_length>
_bz_forceinline
TinyVector<P_numtype, N_length>& 
TinyVector<P_numtype,N_length>::initialize(T_numtype x)
{
    (*this) = _bz_ArrayExpr<_bz_ArrayExprConstant<T_numtype> >(x);
    return *this;
}

/** This function intercepts TinyVector-only expressions and uses a
    much simpler evaluation that is more likely to get totally
    inlined */
template<typename P_numtype, int N_length>
template<typename T_expr, typename T_update>
_bz_forceinline
void
TinyVector<P_numtype,N_length>::_tv_evaluate(const T_expr& expr, T_update)
{
  if ((T_expr::numArrayOperands>0) || 
      (T_expr::numTMOperands>0) ||
      (T_expr::numIndexPlaceholders>0) ) {
    // not TV-only, punt to general evaluate
    _bz_evaluate(*this, expr, T_update());
    return;
  }

  // TV-only. Since TinyVectors can't have funny storage, ordering,
  // stride, or anything, it's now just a matter of evaluating it like
  // in the old vecassign.

  // since we can't resize tinyvectors, there are two options: all
  // vectors have our size or the expression is malformed.
    // Check that all arrays have the same shape
#ifdef BZ_DEBUG
    if (!expr.shapeCheck(shape()))
    {
      if (assertFailMode == false)
      {
        cerr << "[Blitz++] Shape check failed: Module " << __FILE__
             << " line " << __LINE__ << endl
             << "          Expression: ";
        prettyPrintFormat format(true);   // Use terse formatting
        BZ_STD_SCOPE(string) str;
        expr.prettyPrint(str, format);
        cerr << str << endl ;
      }
    }
#endif

  BZPRECHECK(expr.shapeCheck(shape()),
	     "Shape check failed." << endl << "Expression:");

  BZPRECONDITION(expr.isUnitStride(0));
  BZPRECONDITION(T_expr::rank_<=1);
  BZPRECONDITION(T_expr::numIndexPlaceholders==0);

  _tv_evaluate_aligned(data(), expr, T_update());
}


/** This version of the evaluation function is used normally and
    assumes that the TinyVectors have appropriate alignment (as will
    always be the case if they are actual TinyVector objects and not
    created using reinterpret_cast in the chunked_updater. */
template<typename P_numtype, int N_length>
template<typename T_expr, typename T_update>
_bz_forceinline
void
TinyVector<P_numtype,N_length>::
_tv_evaluate_aligned(T_numtype* data, const T_expr& expr, T_update)
{
  //asm("nop;nop;");
#ifdef BZ_TV_EVALUATE_UNROLL_LENGTH
  if(N_length < BZ_TV_EVALUATE_UNROLL_LENGTH)
    _bz_meta_vecAssign<N_length, 0>::fastAssign(data, expr, T_update());
  else
#endif
#pragma ivdep
#pragma vector aligned
    for (int i=0; i < N_length; ++i)
      T_update::update(data[i], expr.fastRead(i));
  //asm("nop;nop;");
}

/** This version of the evaluation function is used when vectorizing
    expressions that we know can't be aligned. The only difference
    with _tv_evaluate_aligned is the compiler pragma that tells the
    compiler it is unaligned. */
template<typename P_numtype, int N_length>
template<typename T_expr, typename T_update>
_bz_forceinline
void
TinyVector<P_numtype,N_length>::
_tv_evaluate_unaligned(T_numtype* data, const T_expr& expr, T_update)
{
  //asm("nop;nop;");
#ifdef BZ_TV_EVALUATE_UNROLL_LENGTH
  if(N_length < BZ_TV_EVALUATE_UNROLL_LENGTH)
    _bz_meta_vecAssign<N_length, 0>::fastAssign(data, expr, T_update());
  else
#endif
#pragma ivdep
#pragma vector unaligned
    for (int i=0; i < N_length; ++i)
      T_update::update(data[i], expr.fastRead(i));
  //asm("nop;nop;");
}

template<typename P_numtype, int N_length> template<typename T_expr>
_bz_forceinline
TinyVector<P_numtype,N_length>&
TinyVector<P_numtype,N_length>::operator=(const ETBase<T_expr>& expr)
{
  _tv_evaluate(_bz_typename asExpr<T_expr>::T_expr(expr.unwrap()), 
	       _bz_update<
	       T_numtype, 
	       _bz_typename asExpr<T_expr>::T_expr::T_result>());
    return *this;
}

#define BZ_TV2_UPDATE(op,name)						\
  template<typename P_numtype, int N_length>				\
  template<typename T>							\
  _bz_forceinline								\
  TinyVector<P_numtype,N_length>&					\
  TinyVector<P_numtype,N_length>::operator op(const T& expr)		\
  {									\
    _tv_evaluate(_bz_typename asExpr<T>::T_expr(expr),			\
		 name<T_numtype,					\
		 _bz_typename asExpr<T>::T_expr::T_result>());		\
    return *this;							\
  }


BZ_TV2_UPDATE(+=, _bz_plus_update)
BZ_TV2_UPDATE(-=, _bz_minus_update)
BZ_TV2_UPDATE(*=, _bz_multiply_update)
BZ_TV2_UPDATE(/=, _bz_divide_update)
BZ_TV2_UPDATE(%=, _bz_mod_update)
BZ_TV2_UPDATE(^=, _bz_xor_update)
BZ_TV2_UPDATE(&=, _bz_bitand_update)
BZ_TV2_UPDATE(|=, _bz_bitor_update)
BZ_TV2_UPDATE(<<=, _bz_shiftl_update)
BZ_TV2_UPDATE(>>=, _bz_shiftr_update)

/*
 * Other member functions
 */

template<typename P_numtype, int N_length>
template<int N0>
_bz_forceinline
_bz_ArrayExpr<ArrayIndexMapping<typename asExpr<TinyVector<P_numtype, N_length> >::T_expr, N0> >
TinyVector<P_numtype, N_length>::operator()(IndexPlaceholder<N0>) const
{ 
        return _bz_ArrayExpr<ArrayIndexMapping<typename asExpr<T_vector>::T_expr, N0> >
            (noConst());
}


BZ_NAMESPACE_END

#include <blitz/tv2fastiter.h>  // Iterators
//#include <blitz/tv2assign.h> unused now?
#include <blitz/tinyvec2io.cc>

#endif // BZ_TINYVEC_CC
