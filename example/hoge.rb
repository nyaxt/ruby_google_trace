require 'tracejson'

TraceJson::TraceLog.hook_tracepoint_enable

def fact(n)
  return 1 if n < 2
  n * fact(n-1)
end

p fact(5)

TraceJson::TraceLog.hook_tracepoint_disable

open("trace.json", "w") {|f| f.write TraceJson::TraceLog.to_json }
