subroutine flat4(size,spectrum,flat) bind(C, name='flat4')

! Flatten a spectrum for optimum display
! Input:  spectrum(size) Linear scale in power
!         flat           If false, convert to dB but do not flatten
! Output: spectrum(size) Flattened, with dB scale


  use iso_c_binding, only: c_bool, c_float, c_int
  implicit real*8 (a-h,o-z)
  integer(c_int), value                         :: size
  real(c_float), dimension(size), intent(inout) :: spectrum
  logical(c_bool), value                        :: flat
  real(c_float) base
  real*8 x(1000),y(1000),a(5)
  data nseg/10/,npct/10/

  do i=1,size
     spectrum(i)=10.0*log10(spectrum(i))            !Convert to dB scale
  enddo

  if(flat) then
     nterms=5
     nlen=size/nseg                   !Length of test segment
     i0=size/2                        !Midpoint
     k=0
     do n=1,nseg                      !Skip first segment, likely rolloff here
        ib=n*nlen
        ia=ib-nlen+1
        if(n.eq.nseg) ib=size
        call pctile(spectrum(ia),ib-ia+1,npct,base) !Find lowest npct of points
        do i=ia,ib
           if(spectrum(i).le.base) then
              if (k.lt.1000) k=k+1    !Save these "lower envelope" points
              x(k)=i-i0
              y(k)=spectrum(i)
           endif
        enddo
     enddo
     kz=k
     a=0.
  
     call polyfit(x,y,y,kz,nterms,0,a,chisqr)  !Fit a low-order polynomial

     do i=1,size
        t=i-i0
        yfit=a(1)+t*(a(2)+t*(a(3)+t*(a(4)+t*(a(5)))))
        spectrum(i)=real(spectrum(i)-yfit, c_float)                !Subtract the fitted baseline
     enddo
  endif

  return
end subroutine flat4
