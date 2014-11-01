require 'json'

module TraceJson
  class TraceLog
    def self.to_json
      pid = Process.pid

      events = to_a.map {|name, cat, args, ph, ts|
        {
          name: name,
          cat: cat,
          args: args || {},
          ph: ph,
          pid: pid,
          tid: 1234, # FIXME
          ts: ts / 1000
        }
      }
      # events.each {|e| p e }

      {
        traceEvents: events,
        otherData: {
          version: "ruby #{RUBY_VERSION}"
        }
      }.to_json
    end
  end
end
