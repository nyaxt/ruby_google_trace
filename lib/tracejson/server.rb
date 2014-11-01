require 'tracejson/tracelog'

require 'json'
require 'webrick'

module TraceJson
  class Server < WEBrick::HTTPServer
    def initialize
      super(Port: 7502)
      mount_proc '/', method(:handle).to_proc
    end

    def handle(req, res)
      json = {hello: :world}

      res.content_type = 'application/json'      
      res.body = json.to_json
      res.content_length = res.body.length
    end
  end
end
