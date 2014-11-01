require 'json'

module TraceJson
  class TraceLog
    def self.to_json
      to_hash.to_json
    end

    def self.to_hash
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

      {
        traceEvents: events,
        otherData: {
          version: "ruby #{RUBY_VERSION}"
        }
      }
    end
  end
end
