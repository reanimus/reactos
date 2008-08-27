#include "precomp.h"

/* the following deal with IEEE single-precision numbers */
#define EXCESS          126L
#define SIGNBIT         0x80000000L
#define SIGN(fp)        ((fp) & SIGNBIT)
#define EXP(fp)         (((fp) >> 23L) & 0xFF)
#define MANT(fp)        ((fp) & 0x7FFFFFL)
#define PACK(s,e,m)     ((s) | ((e) << 23L) | (m))

// Sames as lrintf.
#ifdef __GNUC__
#define FLOAT_TO_INT(in,out)  \
           __asm__ __volatile__ ("fistpl %0" : "=m" (out) : "t" (in) : "st");
#else
#define FLOAT_TO_INT(in,out) \
          __asm fld in \
          __asm fistp out
#endif

LONG
FASTCALL
EFtoF( EFLOAT_S * efp)
{
 long Mant, Exp, Sign = 0;

 if (!efp->lMant) return 0;

 Mant = efp->lMant;
 Exp = efp->lExp;
 Sign = SIGN(Mant);

//// M$ storage emulation
 if( Sign ) Mant = -Mant;
 Mant = ((Mant & 0x3fffffff) >> 7);
 Exp += (EXCESS-1);
////
 Mant = MANT(Mant);
 return PACK(Sign, Exp, Mant);
}

VOID
FASTCALL
FtoEF( EFLOAT_S * efp, FLOATL f)
{
 long Mant, Exp, Sign = 0;
 gxf_long worker;

#ifdef _X86_
 worker.l = f; // It's a float stored in a long.
#else
 worker.f = f;
#endif

 Exp = EXP(worker.l);
 Mant = MANT(worker.l);
 if (SIGN(worker.l)) Sign = -1;
//// M$ storage emulation
 Mant = ((Mant << 7) | 0x40000000);
 Mant ^= Sign;
 Mant -= Sign;
 Exp -= (EXCESS-1);
////
 efp->lMant = Mant;
 efp->lExp = Exp;
}


VOID FASTCALL
CoordCnvP(MATRIX_S * mx, LPPOINT Point)
{
  FLOAT x, y;
  gxf_long a, b, c;

  x = (FLOAT)Point->x;
  y = (FLOAT)Point->y;

  a.l = EFtoF( &mx->efM11 );
  b.l = EFtoF( &mx->efM21 );
  c.l = EFtoF( &mx->efDx  );
  x = x * a.f + y * b.f + c.f;

  a.l = EFtoF( &mx->efM12 );
  b.l = EFtoF( &mx->efM22 );
  c.l = EFtoF( &mx->efDy  );
  y = x * a.f + y * b.f + c.f;

  FLOAT_TO_INT(x, Point->x );
  FLOAT_TO_INT(y, Point->y );
}


BOOL
STDCALL
DPtoLP ( HDC hDC, LPPOINT Points, INT Count )
{
#if 0
  INT i;
  PDC_ATTR Dc_Attr;

  if (!GdiGetHandleUserData((HGDIOBJ) hDC, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;

  if (Dc_Attr->flXform & ( DEVICE_TO_WORLD_INVALID | // Force a full recalibration!
                           PAGE_XLATE_CHANGED      | // Changes or Updates have been made,
                           PAGE_EXTENTS_CHANGED    | // do processing in kernel space.
                           WORLD_XFORM_CHANGED ))
#endif
    return NtGdiTransformPoints( hDC, Points, Points, Count, GdiDpToLp); // DPtoLP mode.
#if 0
  else
  {
    for ( i = 0; i < Count; i++ )
      CoordCnvP ( &Dc_Attr->mxDeviceToWorld, &Points[i] );
  }
  return TRUE;
#endif
}


BOOL
STDCALL
LPtoDP ( HDC hDC, LPPOINT Points, INT Count )
{
#if 0
  INT i;
  PDC_ATTR Dc_Attr;

  if (!GdiGetHandleUserData((HGDIOBJ) hDC, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;

  if (Dc_Attr->flXform & ( PAGE_XLATE_CHANGED   |  // Check for Changes and Updates
                           PAGE_EXTENTS_CHANGED |
                           WORLD_XFORM_CHANGED ))
#endif
    return NtGdiTransformPoints( hDC, Points, Points, Count, GdiLpToDp); // LPtoDP mode
#if 0
  else
  {
    for ( i = 0; i < Count; i++ )
      CoordCnvP ( &Dc_Attr->mxWorldToDevice, &Points[i] );
  }
  return TRUE;
#endif
}

/*
 * @implemented
 *
 */
BOOL
STDCALL
GetCurrentPositionEx(HDC hdc,
                     LPPOINT lpPoint)
{
  PDC_ATTR Dc_Attr;

  if (!GdiGetHandleUserData((HGDIOBJ) hdc, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;

  if ( lpPoint )
  {
    if ( Dc_Attr->ulDirty_ & DIRTY_PTLCURRENT ) // have a hit!
    {
       lpPoint->x = Dc_Attr->ptfxCurrent.x;
       lpPoint->y = Dc_Attr->ptfxCurrent.y;
       DPtoLP ( hdc, lpPoint, 1);          // reconvert back.
       Dc_Attr->ptlCurrent.x = lpPoint->x; // save it
       Dc_Attr->ptlCurrent.y = lpPoint->y;
       Dc_Attr->ulDirty_ &= ~DIRTY_PTLCURRENT; // clear bit
    } 
    else
    {
       lpPoint->x = Dc_Attr->ptlCurrent.x;
       lpPoint->y = Dc_Attr->ptlCurrent.y;
    }
  }
  else
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  return TRUE;                                                             
}

/*
 * @implemented
 */
BOOL
STDCALL
GetWorldTransform( HDC hDC, LPXFORM lpXform )
{
  return NtGdiGetTransform( hDC, GdiWorldSpaceToPageSpace, lpXform);
}


BOOL
STDCALL
SetWorldTransform( HDC hDC, CONST XFORM *Xform )
{
      /* FIXME  shall we add undoc #define MWT_SETXFORM 4 ?? */
      return ModifyWorldTransform( hDC, Xform, MWT_MAX+1);
}


BOOL
STDCALL
ModifyWorldTransform(
                  HDC hDC,
                CONST XFORM *Xform,
                DWORD iMode
                    )
{
#if 0
// Handle something other than a normal dc object.
  if (GDI_HANDLE_GET_TYPE(hDC) != GDI_OBJECT_TYPE_DC)
  {
    if (GDI_HANDLE_GET_TYPE(hDC) == GDI_OBJECT_TYPE_METADC)
      return FALSE;
    else
    {
      PLDC pLDC = GdiGetLDC(hDC);
      if ( !pLDC )
      {
         SetLastError(ERROR_INVALID_HANDLE);
         return FALSE;
      }
      if (pLDC->iType == LDC_EMFLDC)
      {
        if (iMode ==  MWT_MAX+1)
          if (!EMFDRV_SetWorldTransform( hDC, Xform) ) return FALSE;
        return EMFDRV_ModifyWorldTransform( hDC, Xform, iMode); // Ported from wine.
      }
      return FALSE;
    }
  }
#endif
  PDC_ATTR Dc_Attr;

  if (!GdiGetHandleUserData((HGDIOBJ) hDC, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;

  /* Check that graphics mode is GM_ADVANCED */
  if ( Dc_Attr->iGraphicsMode != GM_ADVANCED ) return FALSE;

  return NtGdiModifyWorldTransform(hDC, (CONST LPXFORM) Xform, iMode);
}

BOOL
STDCALL
GetViewportExtEx(
             HDC hdc,
             LPSIZE lpSize
                )
{
  PDC_ATTR Dc_Attr;

  if (!GdiGetHandleUserData((HGDIOBJ) hdc, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;

  if ((Dc_Attr->flXform & PAGE_EXTENTS_CHANGED) && (Dc_Attr->iMapMode == MM_ISOTROPIC))
     // Something was updated, go to kernel.
     return NtGdiGetDCPoint( hdc, GdiGetViewPortExt, (LPPOINT) lpSize );
  else
  {
     lpSize->cx = Dc_Attr->szlViewportExt.cx;
     lpSize->cy = Dc_Attr->szlViewportExt.cy;
  }
  return TRUE;
}


BOOL
STDCALL
GetViewportOrgEx(
             HDC hdc,
             LPPOINT lpPoint
                )
{
  PDC_ATTR Dc_Attr;

  if (!GdiGetHandleUserData((HGDIOBJ) hdc, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;
  lpPoint->x = Dc_Attr->ptlViewportOrg.x;
  lpPoint->y = Dc_Attr->ptlViewportOrg.y;
  if (Dc_Attr->dwLayout & LAYOUT_RTL) lpPoint->x = -lpPoint->x;
  return TRUE;
  // return NtGdiGetDCPoint( hdc, GdiGetViewPortOrg, lpPoint );
}


BOOL
STDCALL
GetWindowExtEx(
           HDC hdc,
           LPSIZE lpSize
              )
{
  PDC_ATTR Dc_Attr;

  if (!GdiGetHandleUserData((HGDIOBJ) hdc, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;
  lpSize->cx = Dc_Attr->szlWindowExt.cx;
  lpSize->cy = Dc_Attr->szlWindowExt.cy;
  if (Dc_Attr->dwLayout & LAYOUT_RTL) lpSize->cx = -lpSize->cx;
  return TRUE;
  // return NtGdiGetDCPoint( hdc, GdiGetWindowExt, (LPPOINT) lpSize );
}


BOOL
STDCALL
GetWindowOrgEx(
           HDC hdc,
           LPPOINT lpPoint
              )
{
  PDC_ATTR Dc_Attr;

  if (!GdiGetHandleUserData((HGDIOBJ) hdc, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;
  lpPoint->x = Dc_Attr->ptlWindowOrg.x;
  lpPoint->y = Dc_Attr->ptlWindowOrg.y;
  return TRUE;
  //return NtGdiGetDCPoint( hdc, GdiGetWindowOrg, lpPoint );
}

/*
 * @unimplemented
 */
BOOL
STDCALL
SetViewportExtEx(HDC hdc,
                 int nXExtent,
                 int nYExtent,
                 LPSIZE lpSize)
{
#if 0
  PDC_ATTR Dc_Attr;
#if 0
  if (GDI_HANDLE_GET_TYPE(hdc) != GDI_OBJECT_TYPE_DC)
  {
    if (GDI_HANDLE_GET_TYPE(hdc) == GDI_OBJECT_TYPE_METADC)
      return MFDRV_SetViewportExtEx();
    else
    {
      PLDC pLDC = GdiGetLDC(hdc);
      if ( !pLDC )
      {
         SetLastError(ERROR_INVALID_HANDLE);
         return FALSE;
      }
      if (pLDC->iType == LDC_EMFLDC)
      {
        return EMFDRV_SetViewportExtEx();
      }
    }
  }
#endif
  if (!GdiGetHandleUserData((HGDIOBJ) hdc, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;

  if (lpSize)
  {
     lpSize->cx = Dc_Attr->szlWindowExt.cx;
     lpSize->cy = Dc_Attr->szlWindowExt.cy;
  }

  if ((Dc_Attr->ptlWindowExt.cx == nXExtent) && (Dc_Attr->ptlWindowExt.cy == nYExtent))
     return TRUE;

  if ((Dc_Attr->iMapMode == MM_ISOTROPIC) && (Dc_Attr->iMapMode == MM_ANISOTROPIC))
  {
     if (NtCurrentTeb()->GdiTebBatch.HDC == (ULONG)hdc)
     {
        if (Dc_Attr->ulDirty_ & DC_FONTTEXT_DIRTY)
        {
           NtGdiFlush(); // Sync up Dc_Attr from Kernel space.
           Dc_Attr->ulDirty_ &= ~(DC_MODE_DIRTY|DC_FONTTEXT_DIRTY);
        }
     }
     Dc_Attr->szlWindowExt.cx = nXExtent;
     Dc_Attr->szlWindowExt.cy = nYExtent;
     if (Dc_Attr->dwLayout & LAYOUT_RTL) NtGdiMirrorWindowOrg(hdc);
     Dc_Attr->flXform |= (PAGE_EXTENTS_CHANGED|INVALIDATE_ATTRIBUTES|DEVICE_TO_WORLD_INVALID);
  }
  return TRUE;
#endif
  /* Unimplemented for now */
  return FALSE;
}

/*
 * @unimplemented
 */
BOOL
STDCALL
SetWindowOrgEx(HDC hdc,
               int X,
               int Y,
               LPPOINT lpPoint)
{
#if 0
  PDC_ATTR Dc_Attr;
#if 0
  if (GDI_HANDLE_GET_TYPE(hdc) != GDI_OBJECT_TYPE_DC)
  {
    if (GDI_HANDLE_GET_TYPE(hdc) == GDI_OBJECT_TYPE_METADC)
      return MFDRV_SetWindowOrgEx();
    else
    {
      PLDC pLDC = GdiGetLDC(hdc);
      if ( !pLDC )
      {
         SetLastError(ERROR_INVALID_HANDLE);
         return FALSE;
      }
      if (pLDC->iType == LDC_EMFLDC)
      {
        return EMFDRV_SetWindowOrgEx();
      }
    }
  }
#endif
  if (!GdiGetHandleUserData((HGDIOBJ) hdc, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;

  if (lpPoint)
  {
     lpPoint->x = Dc_Attr->ptlWindowOrg.x;
     lpPoint->y = Dc_Attr->ptlWindowOrg.y;
  }

  if ((Dc_Attr->ptlWindowOrg.x == X) && (Dc_Attr->ptlWindowOrg.y == Y))
     return TRUE;

  if (NtCurrentTeb()->GdiTebBatch.HDC == (ULONG)hdc)
  {
     if (Dc_Attr->ulDirty_ & DC_FONTTEXT_DIRTY)
     {
       NtGdiFlush(); // Sync up Dc_Attr from Kernel space.
       Dc_Attr->ulDirty_ &= ~(DC_MODE_DIRTY|DC_FONTTEXT_DIRTY);
     }
  }
  
  Dc_Attr->ptlWindowOrg.x = X;
  Dc_Attr->lWindowOrgx    = X;
  Dc_Attr->ptlWindowOrg.y = Y;
  if (Dc_Attr->dwLayout & LAYOUT_RTL) NtGdiMirrorWindowOrg(hdc);
  Dc_Attr->flXform |= (PAGE_XLATE_CHANGED|DEVICE_TO_WORLD_INVALID);
  return TRUE;
#endif
  /* Unimplemented for now */
  return FALSE;
}

/*
 * @unimplemented
 */
BOOL
STDCALL
SetWindowExtEx(HDC hdc,
               int nXExtent,
               int nYExtent,
               LPSIZE lpSize)
{
#if 0
  PDC_ATTR Dc_Attr;
#if 0
  if (GDI_HANDLE_GET_TYPE(hdc) != GDI_OBJECT_TYPE_DC)
  {
    if (GDI_HANDLE_GET_TYPE(hdc) == GDI_OBJECT_TYPE_METADC)
      return MFDRV_SetWindowExtEx();
    else
    {
      PLDC pLDC = GdiGetLDC(hdc);
      if ( !pLDC )
      {
         SetLastError(ERROR_INVALID_HANDLE);
         return FALSE;
      }
      if (pLDC->iType == LDC_EMFLDC)
      {
        return EMFDRV_SetWindowExtEx();
      }
    }
  }
#endif
  if (!GdiGetHandleUserData((HGDIOBJ) hdc, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;

  if (lpSize)
  {
     lpSize->cx = Dc_Attr->szlWindowExt.cx;
     lpSize->cy = Dc_Attr->szlWindowExt.cy;
     if (Dc_Attr->dwLayout & LAYOUT_RTL) lpSize->cx = -lpSize->cx;
  }

  if (Dc_Attr->dwLayout & LAYOUT_RTL)
  {
     NtGdiMirrorWindowOrg(hdc);
     Dc_Attr->flXform |= (PAGE_EXTENTS_CHANGED|INVALIDATE_ATTRIBUTES|DEVICE_TO_WORLD_INVALID);
  }
  else if ((Dc_Attr->iMapMode == MM_ISOTROPIC) && (Dc_Attr->iMapMode == MM_ANISOTROPIC))
  {
     if ((Dc_Attr->szlWindowExt.cx == nXExtent) && (Dc_Attr->szlWindowExt.cy == nYExtent))
        return TRUE;

     if ((!nXExtent) && (!nYExtent)) return FALSE;

     if (NtCurrentTeb()->GdiTebBatch.HDC == (ULONG)hdc)
     {
        if (Dc_Attr->ulDirty_ & DC_FONTTEXT_DIRTY)
        {
           NtGdiFlush(); // Sync up Dc_Attr from Kernel space.
           Dc_Attr->ulDirty_ &= ~(DC_MODE_DIRTY|DC_FONTTEXT_DIRTY);
        }
     }
     Dc_Attr->szlWindowExt.cx = nXExtent;
     Dc_Attr->szlWindowExt.cy = nYExtent;
     if (Dc_Attr->dwLayout & LAYOUT_RTL) NtGdiMirrorWindowOrg(hdc);
     Dc_Attr->flXform |= (PAGE_EXTENTS_CHANGED|INVALIDATE_ATTRIBUTES|DEVICE_TO_WORLD_INVALID);  
  }
  return TRUE; // Return TRUE.
#endif
  /* Unimplemented for now */
  return FALSE;
}

/*
 * @unimplemented
 */
BOOL
STDCALL
SetViewportOrgEx(HDC hdc,
                 int X,
                 int Y,
                 LPPOINT lpPoint)
{
#if 0
  PDC_ATTR Dc_Attr;
#if 0
  if (GDI_HANDLE_GET_TYPE(hdc) != GDI_OBJECT_TYPE_DC)
  {
    if (GDI_HANDLE_GET_TYPE(hdc) == GDI_OBJECT_TYPE_METADC)
      return MFDRV_SetViewportOrgEx();
    else
    {
      PLDC pLDC = GdiGetLDC(hdc);
      if ( !pLDC )
      {
         SetLastError(ERROR_INVALID_HANDLE);
         return FALSE;
      }
      if (pLDC->iType == LDC_EMFLDC)
      {
        return EMFDRV_SetViewportOrgEx();
      }
    }
  }
#endif
  if (!GdiGetHandleUserData((HGDIOBJ) hdc, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return FALSE;

  if (lpPoint)
  {
     lpPoint->x = Dc_Attr->ptlViewportOrg.x;
     lpPoint->y = Dc_Attr->ptlViewportOrg.y;
     if (Dc_Attr->dwLayout & LAYOUT_RTL) lpPoint->x = -lpPoint->x;
  }
  Dc_Attr->flXform |= (PAGE_XLATE_CHANGED|DEVICE_TO_WORLD_INVALID);
  if (Dc_Attr->dwLayout & LAYOUT_RTL) X = -X;
  Dc_Attr->ptlViewportOrg.x = X;
  Dc_Attr->ptlViewportOrg.y = Y;
  return TRUE;
#endif
  /* Unimplemented for now */
  return FALSE;
}

/*
 * @implemented
 */
BOOL
STDCALL
ScaleViewportExtEx(
	HDC	a0,
	int	a1,
	int	a2,
	int	a3,
	int	a4,
	LPSIZE	a5
	)
{
#if 0
  if (GDI_HANDLE_GET_TYPE(a0) != GDI_OBJECT_TYPE_DC)
  {
    if (GDI_HANDLE_GET_TYPE(a0) == GDI_OBJECT_TYPE_METADC)
      return MFDRV_;
    else
    {
      PLDC pLDC = GdiGetLDC(a0);
      if ( !pLDC )
      {
         SetLastError(ERROR_INVALID_HANDLE);
         return FALSE;
      }
      if (pLDC->iType == LDC_EMFLDC)
      {
        return EMFDRV_;
      }
    }
  }
#endif
  if (!GdiIsHandleValid((HGDIOBJ) a0) ||
      (GDI_HANDLE_GET_TYPE(a0) != GDI_OBJECT_TYPE_DC)) return FALSE;

  return NtGdiScaleViewportExtEx(a0, a1, a2, a3, a4, a5);
}

/*
 * @implemented
 */
BOOL
STDCALL
ScaleWindowExtEx(
	HDC	a0,
	int	a1,
	int	a2,
	int	a3,
	int	a4,
	LPSIZE	a5
	)
{
#if 0
  if (GDI_HANDLE_GET_TYPE(a0) != GDI_OBJECT_TYPE_DC)
  {
    if (GDI_HANDLE_GET_TYPE(a0) == GDI_OBJECT_TYPE_METADC)
      return MFDRV_;
    else
    {
      PLDC pLDC = GdiGetLDC(a0);
      if ( !pLDC )
      {
         SetLastError(ERROR_INVALID_HANDLE);
         return FALSE;
      }
      if (pLDC->iType == LDC_EMFLDC)
      {
        return EMFDRV_;
      }
    }
  }
#endif
  if (!GdiIsHandleValid((HGDIOBJ) a0) ||
      (GDI_HANDLE_GET_TYPE(a0) != GDI_OBJECT_TYPE_DC)) return FALSE;

  return NtGdiScaleWindowExtEx(a0, a1, a2, a3, a4, a5);
}

/*
 * @implemented
 */
DWORD
STDCALL
GetLayout(HDC hdc
)
{
  PDC_ATTR Dc_Attr;
  if (!GdiGetHandleUserData((HGDIOBJ) hdc, GDI_OBJECT_TYPE_DC, (PVOID) &Dc_Attr)) return GDI_ERROR;
  return Dc_Attr->dwLayout;
}


/*
 * @implemented
 */
DWORD
STDCALL
SetLayout(HDC hdc,
          DWORD dwLayout)
{
#if 0
  if (GDI_HANDLE_GET_TYPE(hdc) != GDI_OBJECT_TYPE_DC)
  {
    if (GDI_HANDLE_GET_TYPE(hdc) == GDI_OBJECT_TYPE_METADC)
      return MFDRV_SetLayout( hdc, dwLayout);
    else
    {
      PLDC pLDC = GdiGetLDC(hdc);
      if ( !pLDC )
      {
         SetLastError(ERROR_INVALID_HANDLE);
         return 0;
      }
      if (pLDC->iType == LDC_EMFLDC)
      {
        return EMFDRV_SetLayout( hdc, dwLayout);
      }
    }
  }
#endif
  if (!GdiIsHandleValid((HGDIOBJ) hdc) ||
      (GDI_HANDLE_GET_TYPE(hdc) != GDI_OBJECT_TYPE_DC)) return GDI_ERROR;
  return NtGdiSetLayout( hdc, -1, dwLayout);
}

/*
 * @implemented
 */
DWORD
STDCALL
SetLayoutWidth(HDC hdc,LONG wox,DWORD dwLayout)
{
  if (!GdiIsHandleValid((HGDIOBJ) hdc) || 
      (GDI_HANDLE_GET_TYPE(hdc) != GDI_OBJECT_TYPE_DC)) return GDI_ERROR;
  return NtGdiSetLayout( hdc, wox, dwLayout);
}


