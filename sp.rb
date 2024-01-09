module IEEE_754

  class Sp

    attr_reader :float,:hexa,:int,:sign_bit,:expo_bits,:mant_bits,:hex_string
    def initialize num
      case num
      when Float
        @float=num
        @hexa=[num].pack('g').unpack("H8").first
        @int=@hexa.to_i(16)
      when Integer
        @hexa=num.to_s(16).rjust(8,'0')
        @int=num
      end
      # extract bits :
      @sign_bit =@int[31]
      @expo_bits=@int[23..30]
      @mant_bits=@int[ 0..22]
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
      # insert hidden leading bit, before doing addition of mantisses :
      mant_a= (1 << 23) | a.mant_bits # 24 bits always.
      mant_b= (1 << 23) | b.mant_bits # 24 bits always.

      # perform multiplication. Result on 48 or 47 bits
      mult  = mant_a * mant_b

      # rounding : NIY
      bit_g,bit_r,bit_s=mult[23],mult[22],mult[21]
       #puts "g=#{bit_g},r=#{bit_r},s=#{bit_s}"

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

    # auxilliary function to deduce grs bits while aligning exponents :
    def shift_mantissa mant,nb_shift
    end

    def +(b)
      a=self

      # insert hidden leading bit, before doing addition of mantisses :
      p mant_a= (1 << 23) | a.mant_bits # 24 bits.
      p mant_b= (1 << 23) | b.mant_bits # 24 bits.

      expo_a=a.expo_bits
      expo_b=b.expo_bits
      puts "expo_a=#{expo_a}"
      puts "expo_b=#{expo_b}"
      diff=(a.expo_bits-b.expo_bits).abs
      sign=a.sign_bit

      lsb_bits=[]
      # Rewrite the smaller number such that its exponent matches with the exponent of the larger number.
      if a.expo_bits > b.expo_bits
        return a if (diff > 2**8) # b too small to affect the result.
        # keep track of bits beyond 23+1 bits
        diff.times do |i|
          lsb_bits << mant_b[i]
          mant_b=mant_b >> 1
        end
        expo=expo_a
      elsif a.expo_bits < b.expo_bits
        return b if (diff > 2**8) # a too small to affect the result.
        diff.times do |i|
          lsb_bits << mant_a[i]
          mant_a=mant_a >> 1
        end
        expo=expo_b
      else
        sign=b.sign_bit
        expo=expo_b
      end

      puts "mant_a=#{mant_a} mant_b=#{mant_b}"
      case [a.sign_bit,b.sign_bit]
      when [0,0]
        add=mant_a+mant_b
      when [0,1]
        puts :a_moins_b
        add=mant_a-mant_b
      when [1,0]
        add=-mant_a+mant_b
      when [1,1]
        add=-(mant_a+mant_b)
      end
      puts "add=#{add}"
      sign=add>0 ? 0 : 1

      # normalize
      if add[24]==1
        expo+=1
        lsb_bits << add[0]
        add=add >> 1
      end

      # rounding
      puts "lsb bits : #{lsb_bits}"
      r_bit=lsb_bits.pop
      s_bit=lsb_bits.any?{|b| b==1} ? 1 : 0
      m_bit=add[0]
      mrs=[m_bit,r_bit,s_bit]
      puts "MRS=#{mrs}"
      case mrs
      when [0,0,0]
      when [0,0,1]
      when [0,1,0]
      when [0,1,1]
        add|=0b1
      when [1,0,0]
        add|=0b1
      when [1,0,1]
        add|=0b1
      when [1,1,0]
        add+=1
      when [1,1,1]
        add+=1
      end

      # realign
      # [?] no consequence on sticky bits ?
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
