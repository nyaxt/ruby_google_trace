require File.join(File.dirname(__FILE__), 'lib', 'tracejson', 'version')

Gem::Specification.new do |s|
  s.name = 'tracejson'
  s.version = TraceJson::VERSION
  s.platform = Gem::Platform::RUBY
  s.summary = 'Trace ruby program and output as Google Trace Event Format'
  s.description = 'Trace ruby program and output as Google Trace Event Format'
  s.licenses = ["Ruby"]

  s.authors = ["Kouhei Ueno"]
  s.email = ["ueno@nyaxtstep.com"]
  s.homepage = 'https://github.com/nyaxt/tracejson'

  s.files = `git ls-files`.split("\n")
  s.extensions = ['ext/extconf.rb']
end
