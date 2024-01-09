require "colorize"

LIMIT_MAX=3.4028235    * 10**38
LIMIT_MIN=1.1754942107 * 10**(-38)

HANDWRITTEN_TESTS=[
  {[{6.96875               => 0x40df0000},{-0.3418                => 0xbeaf0069}] => {6.62695                => 0x40D40FF9}},
  {[{0.1235                => 0x3dfced91},{13.45                  => 0x41573333}] => {13.5735                => 0x41592D0E}},
  {[{6.135e-35             => 0x06A318A5},{5.52e+27               => 0x6D8EB04C}] => {5.52e+27               => 0x6D8EB04C}},
]

def gen_random_tests nb
  tests=HANDWRITTEN_TESTS
  nb_gen=0
  while nb_gen < nb  do
    #a_f,b_f=rand_f(),rand_f()
    a_f=rand_f()
    b_f=a_f*rand*rand(1..1000)
    a_hex=f2hex(a_f)
    b_hex=f2hex(b_f)
    e_f=a_f + b_f
    next if e_f > LIMIT_MAX
    next if e_f < LIMIT_MIN
    res_f,res_hex=add(a_f,b_f).first
    args=[{a_f=>f2hex(a_f).to_i(16)}, {b_f => f2hex(b_f).to_i(16)}]
    expected={e_f=>f2hex(e_f).to_i(16)}
    h={args => expected}
    tests << h
    nb_gen+=1
  end
  return tests
end

def f2hex f
  [f].pack('g').unpack("H8").first
end

def i2hex x
  "0x"+x.to_s(16).rjust(8,'0')
end

def rand_f expo=nil
  sign=rand(2)==0 ? -1 : +1
  magn=1+rand.round(2)
  expo||=rand(-126..127).round
  numb=sign*magn*2**expo
end


require_relative 'sp'

def add a_f,b_f
  a=IEEE_754::Sp.new(a_f)
  b=IEEE_754::Sp.new(b_f)
  res=a+b
  {res.float => res.int}
end

if ARGV.empty?
  puts "NOTE : you can give a number of random tests as parameters ! eg 100"
end

nb_tests=ARGV.first.to_i || 100
tests=gen_random_tests(nb_tests)

puts "float".center(76)+"|"+"expected hexa".center(34)+"|"+"actual hexa".center(13)
stats={}
stats["="]=0
stats["~"]=0
stats["x"]=0

tests.each.with_index do |test,idx|
  args,expected=test.first
  a_h,b_h=args
  a_f,a_i=a_h.first
  b_f,b_i=b_h.first
  e_f,e_i=expected.first
  m_f,m_i=add(a_f,b_f).first
  print "#{a_f} + #{b_f}=#{e_f}".ljust(76)+"| "+"#{i2hex(a_i)} #{i2hex(b_i)} #{i2hex(e_i)} | #{i2hex(m_i)}"
  case (e_i-m_i).abs
  when 0
    puts " =".green
    stats["="]+=1
  when 1
    puts " ~".yellow
    stats["~"]+=1
  else
    #print "#{a_f} + #{b_f}=#{e_f}".ljust(76)+"| "+"#{i2hex(a_i)} #{i2hex(b_i)} #{i2hex(e_i)} | #{i2hex(m_i)}"
    puts " x".red
    stats["x"]+=1
  end
end

VERBOSE=false
puts "statistics".center(130,'=')
stats.each do |k,v|
  puts "#{k.ljust(20,'.')} #{v}" if VERBOSE
end
puts
correct   =100.0*stats["="]/stats.values.sum
acceptable=100.0*(stats["="]+stats["~"])/stats.values.sum
errors    =100.0*(stats["x"])/stats.values.sum
puts "correct               = #{correct.round(4)} %"
puts "correct or acceptable = #{acceptable.round(4)} %"
puts "errors                = #{errors.round(4)} %"
