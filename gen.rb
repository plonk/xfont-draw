COL_SJIS = 0
COL_0208 = 1
COL_UNICODE = 2

unicode_to_0208 = Array.new(0x10000)

ARGF.each_line do |line|
  line.sub!(/#.*$/, '')
  line.chomp!
  next if line.empty?
  row = line.split.map { |s| s.to_i(16) }
  cp = row[COL_UNICODE]
  jis = row[COL_0208]
  unicode_to_0208[cp] = jis
end

puts "uint16_t UnicodeToJisx0208[] = {\n"
unicode_to_0208.each_slice(8) do |codes|
  printf "    "
  codes.map! { |c| c ? c : 0 }
  puts codes.map { |c| "0x%04X, " % c }.join
end
puts "};"
