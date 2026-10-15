! This kernel is part of the executable memory restore path, not an example.
! Column-major bit blocks hold 32 logical-Z eigenvalues per synaptic weight.
module neuro_qmemory_decode
  use, intrinsic :: iso_c_binding, only: c_int8_t, c_int32_t, c_size_t, c_int
  implicit none
contains
  ! The certified [[5,1,3]] code has one unique syndrome for each
  ! single-qubit Pauli error. Codes match pq_five_correct: 3*q+1 is X,
  ! 3*q+2 is Z, and 3*q+3 is Y. No multi-error inference is made here.
  subroutine nm_qmemory_decode_observed(syndromes, raw_logical, expected, count, &
       logical, corrections, weights, status) bind(C, name="nm_qmemory_decode_observed")
    integer(c_int8_t), intent(in) :: syndromes(*), raw_logical(*), expected(*)
    integer(c_size_t), value :: count
    integer(c_int8_t), intent(out) :: logical(*), corrections(*)
    integer(c_int32_t), intent(out) :: weights(*)
    integer(c_int), intent(out) :: status
    integer(c_int8_t), parameter :: code(0:15) = [ &
         0_c_int8_t, 4_c_int8_t, 14_c_int8_t, 7_c_int8_t, &
         8_c_int8_t, 2_c_int8_t, 10_c_int8_t, 9_c_int8_t, &
         1_c_int8_t, 11_c_int8_t, 5_c_int8_t, 6_c_int8_t, &
         13_c_int8_t, 3_c_int8_t, 15_c_int8_t, 12_c_int8_t ]
    integer(c_size_t) :: edge, bit, index
    integer(c_int32_t) :: value, recovered
    integer(c_int) :: syndrome, read_bit, expected_bit, correction, flip
    status = 0
    if (count == 0_c_size_t) then
      status = 1
      return
    end if
    do edge = 0_c_size_t, count - 1_c_size_t
      value = 0_c_int32_t
      do bit = 0_c_size_t, 31_c_size_t
        index = 32_c_size_t*edge + bit + 1_c_size_t
        syndrome = int(syndromes(index), c_int)
        read_bit = int(raw_logical(index), c_int)
        expected_bit = int(expected(index), c_int)
        if (syndrome < 0 .or. syndrome > 15 .or. &
            read_bit < 0 .or. read_bit > 1 .or. &
            expected_bit < 0 .or. expected_bit > 1) then
          status = 1
          return
        end if
        correction = int(code(syndrome), c_int)
        flip = 0
        if (correction /= 0) then
          if (mod(correction - 1, 3) /= 1) flip = 1
        end if
        recovered = ieor(read_bit, flip)
        if (recovered /= expected_bit) then
          status = 2
          return
        end if
        logical(index) = int(recovered, c_int8_t)
        corrections(index) = int(correction, c_int8_t)
        value = ior(value, ishft(recovered, int(bit)))
      end do
      weights(edge + 1_c_size_t) = value
    end do
  end subroutine nm_qmemory_decode_observed

  subroutine nm_qmemory_decode(bits, count, weights, status) bind(C, name="nm_qmemory_decode")
    integer(c_int8_t), intent(in) :: bits(*)
    integer(c_size_t), value :: count
    integer(c_int32_t), intent(out) :: weights(*)
    integer(c_int), intent(out) :: status
    integer(c_size_t) :: edge, bit
    integer(c_int32_t) :: value, digit
    status = 0
    do edge = 0_c_size_t, count - 1_c_size_t
      value = 0_c_int32_t
      do bit = 0_c_size_t, 31_c_size_t
        digit = int(bits(32_c_size_t*edge + bit + 1_c_size_t), c_int32_t)
        if (iand(digit, not(1_c_int32_t)) /= 0_c_int32_t) then
          status = 1
          return
        end if
        value = ior(value, ishft(digit, int(bit)))
      end do
      weights(edge + 1_c_size_t) = value
    end do
  end subroutine nm_qmemory_decode
end module neuro_qmemory_decode
