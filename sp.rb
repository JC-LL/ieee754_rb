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
      [@sign_bit,@expo_bits.to_s(2).rjust(8,'0'),@mant_bits.to_s(2).rjust(23,'0')].join(blank)
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


    def +(b)
      a=self
      #puts a.hex_string+" "+a.bit_string
      #puts b.hex_string+" "+b.bit_string

      mant_a= (1 << 23) | a.mant_bits # 24 bits always.
      mant_b= (1 << 23) | b.mant_bits # 24 bits always.

      expo_a=a.expo_bits
      expo_b=b.expo_bits

      diff=(a.expo_bits-b.expo_bits).abs
      sign=a.sign_bit

      # Rewrite the smaller number such that its exponent matches with the exponent of the larger number.
      if a.expo_bits > b.expo_bits
        return a if (diff > 2**8) # b to small to affect the result.
        mant_b=mant_b >> diff
        sign=a.sign_bit
        expo=expo_a
      elsif a.expo_bits < b.expo_bits
        return b if (diff > 2**8) # a to small to affect the result.
        mant_a=mant_a >> diff
        sign=b.sign_bit
        expo=expo_b
      else
        sign=b.sign_bit
        expo=expo_b
      end

      case [a.sign_bit,b.sign_bit]
      when [0,0]
        add=mant_a+mant_b
      when [0,1]
        add=mant_a-mant_b
      when [1,0]
        add=-mant_a+mant_b
      when [1,1]
        add=-(mant_a+mant_b)
      end

      # normalize
      if add[24]==1
        expo+=1
        add=add >> 1
      end

      while add[23]!=1
        expo-=1
        add=add << 1
      end

      # get mantissa without leading 1
      mant_a=add & 0x7fffff
      # final packing
      int=(sign << 31) | (expo << 23) | mant_a
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
