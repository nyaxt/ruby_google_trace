require 'tracejson/server'

Thread.new { TraceJson::Server.new.start }
