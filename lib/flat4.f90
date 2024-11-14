subroutine flat4(s,npts,bflatten)

! Flatten a spectrum for optimum display
! Input:  s(npts)    Linear scale in power
!         bflatten   If false, convert to dB but do not flatten
! Output: s(npts)    Flattened, with dB scale


  use iso_c_binding, only: c_bool, c_float, c_int
  implicit real*8 (a-h,o-z)
  logical(c_bool), value       :: bflatten
  integer(c_int), value        :: npts
  real(c_float), intent(inout) :: s(npts);
  real(c_float) base
  real*8 x(1000),y(1000),a(5)
  data nseg/10/,npct/10/

  do i=1,npts
     s(i)=10.0*log10(s(i))            !Convert to dB scale
  enddo

  if(bflatten) then
     nterms=5
     nlen=npts/nseg                   !Length of test segment
     i0=npts/2                        !Midpoint
     k=0
     do n=1,nseg                      !Skip first segment, likely rolloff here
        ib=n*nlen
        ia=ib-nlen+1
        if(n.eq.nseg) ib=npts
        call pctile(s(ia),ib-ia+1,npct,base) !Find lowest npct of points
        do i=ia,ib
           if(s(i).le.base) then
              if (k.lt.1000) k=k+1    !Save these "lower envelope" points
              x(k)=i-i0
              y(k)=s(i)
           endif
        enddo
     enddo
     kz=k
     a=0.
  
     call polyfit(x,y,y,kz,nterms,0,a,chisqr)  !Fit a low-order polynomial

     do i=1,npts
        t=i-i0
        yfit=a(1)+t*(a(2)+t*(a(3)+t*(a(4)+t*(a(5)))))
        s(i)=s(i)-yfit                !Subtract the fitted baseline
     enddo
  endif

  return
end subroutine flat4
