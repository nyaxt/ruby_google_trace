require 'webrick'

def fact(n)
  return 1 if n < 2
  n * fact(n-1)
end

s = WEBrick::HTTPServer.new(Port: 8080)
s.mount_proc('/') do |req, res|
  unless req.path =~ /(\d+)$/
    res.body = 'parse fail'
    res.status = 400
    break
  end

  n = $1.to_i
  r = fact(n)
  res.body = r.to_s
  res.content_type = 'text/plain'
end
s.start
