#ifndef DASH__VIEW__SUB_H__INCLUDED
#define DASH__VIEW__SUB_H__INCLUDED

#include <dash/Types.h>
#include <dash/Range.h>

#include <dash/view/ViewMod.h>
#include <dash/view/NViewMod.h>


namespace dash {

// -------------------------------------------------------------------------
// View Modifiers (not coupled with origin memory / index space):
// -------------------------------------------------------------------------

// Sub-space slice, view dimensions maintain domain dimensions

/**
 * \concept{DashViewConcept}
 */
template <
  dim_t SubDim   = 0,
  dim_t NViewDim,
  class OffsetT >
constexpr ViewSubMod<ViewOrigin<NViewDim>, SubDim>
sub(
    OffsetT begin,
    OffsetT end) {
  return ViewSubMod<ViewOrigin<NViewDim>, SubDim>(begin, end);
}

/**
 * \concept{DashViewConcept}
 */
template <
  dim_t SubDim   = 0,
  dim_t NViewDim,
  class IndexRangeT >
constexpr ViewSubMod<ViewOrigin<NViewDim>, SubDim>
sub(const IndexRangeT & range) {
  return sub<SubDim>(dash::begin(range),
                     dash::end(range));
}

// Sub-space projection, view reduces domain by one dimension

#if 0
/**
 * \concept{DashViewConcept}
 */
template <
  dim_t SubDim = 0,
  class OffsetT >
constexpr ViewSubMod<ViewOrigin, SubDim>
sub(
    OffsetT offset) {
  return ViewSubMod<ViewOrigin, SubDim>(offset);
}
#endif

// -------------------------------------------------------------------------
// View Proxies (coupled with origin memory / index space):
// -------------------------------------------------------------------------

// Sub-space slice, view dimensions maintain domain dimensions

/**
 * \concept{DashViewConcept}
 */
template <
  dim_t SubDim  = 0,
  class DomainT,
  class OffsetT >
constexpr auto
sub(
    OffsetT         begin,
    OffsetT         end,
    const DomainT & domain)
  -> typename std::enable_if<
       dash::view_traits<DomainT>::rank::value == 1,
       ViewSubMod<DomainT, SubDim>
     >::type {
  return ViewSubMod<DomainT, SubDim>(
           domain,
           begin,
           end);
}

// =========================================================================
// Multidimensional Views
// =========================================================================

template <
  dim_t SubDim  = 0,
  class DomainT,
  class OffsetT >
constexpr auto
sub(
    OffsetT         begin,
    OffsetT         end,
    const DomainT & domain)
  -> typename std::enable_if<
       (dash::view_traits<DomainT>::rank::value > 1),
       NViewSubMod<DomainT, SubDim, dash::view_traits<DomainT>::rank::value>
     >::type {
  return NViewSubMod<
           DomainT,
           SubDim,
           dash::view_traits<DomainT>::rank::value
         >(domain,
           begin,
           end);
}

} // namespace dash

#endif // DASH__VIEW__SUB_H__INCLUDED
