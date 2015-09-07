require 'text/hyphen'

hh = Text::Hyphen.new
while line = gets
  line.chop!
  puts hh.hyphenate(line).join(' ')
  STDOUT.flush
end
