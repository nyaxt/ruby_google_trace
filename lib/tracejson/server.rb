require 'tracejson'

require 'json'
require 'webrick'

module TraceJson
  class Server < WEBrick::HTTPServer
    def initialize
      super(Port: 7502)
      mount_proc '/trace.json', method(:handle_json).to_proc
      mount_proc '/tp_enable', method(:handle_tp_enable).to_proc
      mount_proc '/tp_disable', method(:handle_tp_disable).to_proc
    end

    def handle_json(req, res)
      return_as_json(res, TraceLog.to_hash)
    end

    def handle_tp_enable(req, res)
      TraceJson::TraceLog.hook_tracepoint_enable

      return_as_json(res, {ok: 'success'})
    end

    def handle_tp_disable(req, res)
      TraceJson::TraceLog.hook_tracepoint_disable

      return_as_json(res, {ok: 'success'})
    end

    def return_as_json(res, hash)
      res.content_type = 'application/json'      
      res.body = hash.to_json
      res.content_length = res.body.length
    end
  end
end
