  use, intrinsic :: iso_c_binding, only: c_int, c_short, c_float, c_char, c_bool

  include 'constants.f90'

  !
  ! these structures must be kept in sync with ../commons.h
  !
  type, bind(C) :: params_block
     integer(c_int) :: nutc
     logical(c_bool) :: ndiskdat
     integer(c_int) :: ntr
     integer(c_int) :: nQSOProgress ! See MainWindow::m_QSOProgress for values XXX always 0
     integer(c_int) :: nfqso
     integer(c_int) :: nftx
     logical(c_bool) :: newdat
     integer(c_int) :: npts8
     integer(c_int) :: nfa
     integer(c_int) :: nfb
     integer(c_int) :: ntol
     logical(c_bool) :: syncStats
     integer(c_int) :: kin
     integer(c_int) :: kposA
     integer(c_int) :: kposB
     integer(c_int) :: kposC
     integer(c_int) :: kposE
     integer(c_int) :: kposI
     integer(c_int) :: kszA
     integer(c_int) :: kszB
     integer(c_int) :: kszC
     integer(c_int) :: kszE
     integer(c_int) :: kszI
     integer(c_int) :: nzhsym
     integer(c_int) :: nsubmode
     integer(c_int) :: nsubmodes
     logical(c_bool) :: nagain
     integer(c_int) :: ndepth
     logical(c_bool) :: lft8apon 
     logical(c_bool) :: lapcqonly
     logical(c_bool) :: ljt65apon 
     integer(c_int) :: napwid
     integer(c_int) :: ntxmode
     integer(c_int) :: nmode
     integer(c_int) :: minw
     logical(c_bool) :: nclearave
     integer(c_int) :: minsync
     real(c_float) :: emedelay
     real(c_float) :: dttol
     integer(c_int) :: nlist
     integer(c_int) :: listutc(10)
     integer(c_int) :: n2pass
     integer(c_int) :: nranera
     integer(c_int) :: naggressive
     logical(c_bool) :: nrobust
     integer(c_int) :: nexp_decode
     character(kind=c_char) :: datetime(20)
     character(kind=c_char) :: mycall(12)
     character(kind=c_char) :: mygrid(6)
     character(kind=c_char) :: hiscall(12)
     character(kind=c_char) :: hisgrid(6)
     integer(c_int) :: ndebug
  end type params_block

  type, bind(C) :: dec_data
     real(c_float) :: ss(184,NSMAX)
     real(c_float) :: savg(NSMAX)
     real(c_float) :: sred(5760)
     integer(c_short) :: id2(NMAX)
     type(params_block) :: params
  end type dec_data
