module IEEE_754
  class Sp
    attr_reader :float,:hexa,:int,:sign_bit,:expo_bits,:mant_bits,:hex_string
    def initialize num
      case num
      when Float
        @float=num
        @hexa=[num].pack('g').unpack("H8").first
        @int=@hexa.to_i(16)
        @sign_bit =@int[31]
        @expo_bits=@int[23..30]
        @mant_bits=@int[ 0..22]
      when Integer
        @hexa=num.to_s(16).rjust(8,'0')
        @int=num
        @sign_bit =@int[31]
        @expo_bits=@int[23..30]
        @mant_bits=@int[ 0..22]
      end
    end

    def bit_string with_blanks=true
      blank=with_blanks ? " ":""
      [@sign_bit,@expo_bits.to_s(2),@mant_bits.to_s(2)].join(blank)
    end

    def hex_string with_0x=true
      return "0x#{@hexa}" if with_0x
    end

    def *(b)
      a=self
      sign=a.sign_bit ^b.sign_bit
      expo=a.expo_bits + b.expo_bits - 127
      mant_a= (1 << 23) | a.mant_bits # 24 bits always.
      mant_b= (1 << 23) | b.mant_bits # 24 bits always.
      mult  = mant_a * mant_b         # 48 or 47 bits

      # rounding :
      grs=mult[21..23]

      # normalize
      while mult[47]!=1
        expo-=1
        mult=mult << 1
      end
      expo+=1

      # get the appropriate 24 bits : {1+47} => {1+47}-24 ={1+23}
      mult = mult >> 24

      # get mantissa without leading 1
      mant_m=mult & 0x7fffff
      #p mant_m.to_s(2)
      int  = (sign << 31) | (expo << 23) | mant_m
      Sp.new(int)
    end
  end
end

if $PROGRAM_NAME==__FILE__
  p a=IEEE_754::Sp.new(a_f=6.96875)
  puts a.bit_string
  puts a.hex_string

  p b=IEEE_754::Sp.new(b_f=-0.3418)
  puts b.bit_string
  puts b.hex_string

  puts "expected".center(40,'-')
  p e=IEEE_754::Sp.new(a_f*b_f)
  puts e.bit_string
  puts e.hex_string

  puts "actual".center(40,'-')
  p a*b
end
