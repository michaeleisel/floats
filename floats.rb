def float_adders
  highs = []
  lows = []

  (-200..200).to_a.reverse.map do |i|
    pow = (2 ** i).to_s
    padding = "0" * [17 - pow.length, 0].max
    pow = padding + pow
    strs = pow[1...17].each_char.map { |c| "0#{c}" }
    highs << "0x#{strs[0...8].join("")}"
    lows << "0x#{strs[8...16].join("")}"
    # "((__uint128_t) << 64) | 0x#{strs[8...16].join("")}"
  end

  puts "HIGHS\n\n\n"
  puts highs.join(",\n")
  puts "\n\nLOWS\n\n\n"
  puts lows.join(",\n")
end

def pow10_mantissa_and_exp
  nums = (-200..200).map do |i|
    big = ((10 ** i) * (2 ** 1000)).to_i.to_s(2)
    raise if big.length < 64
    s = big[0...64]
    pow = big.length - 1000

    "{0x#{s.to_i(2).to_s(16)}, #{pow}}"
  end

  puts nums.join(",\n")
end

def float_cmps(f)
  str = [f].pack('D').bytes.reverse.map{|n| "%08b" %n }.join
  is_negative = str[0] == '1'
  exp = str[1..11].to_i(2)
  mantissa = ("1" + str[12..63]).to_i(2)
  [is_negative, exp, mantissa]
end

def lead_pad(s, n)
  pad_count = [n - s.length, 0].max
  ("0" * pad_count) + s
end

def pow2topow10_pairs
  pairs = (-200..200).map do |i|
    n = (2 ** i)
    exp = n.to_s.length
    next [exp, 2 ** 64 - 1] if exp == (2 * n).to_s.length
    f = ("1" + ("0" * exp)).to_f
    _, _, mantissa = float_cmps(f)
    [exp, mantissa]
  end
  pairs.map do |exp, mantissa|
    hex = mantissa.to_i.to_s(16)
    "{0x#{lead_pad(hex, 16)}, #{exp}}"
  end.join(",\n")
end

def n_to_s_cache
  size = 4
  (0...(10**size)).map do |i|
    str = lead_pad(i.to_s, size)
    chars = str.each_char.map { |char| "'#{char}'" }.join(",")
    "{#{chars}}"
  end.each_slice(5).map { |s| s.join(", ") }.join(",\n")
end

puts n_to_s_cache
# puts pow10_mantissa_and_exp
