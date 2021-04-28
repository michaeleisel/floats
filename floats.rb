highs = []
lows = []

(0..200).to_a.reverse.map do |i|
  pow = (2 ** i).to_s
  padding = "0" * [17 - pow.length, 0].max
  pow = padding + pow
  strs = pow[1...17].each_char.map { |c| "0#{c}" }
  highs << "0x#{strs[0...8].join("")}"
  lows << "0x#{strs[8...16].join("")}"
end

puts "HIGHS\n\n\n"
puts highs.join(",\n")
puts "\n\nLOWS\n\n\n"
puts lows.join(",\n")
