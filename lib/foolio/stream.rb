# -*- mode:ruby; coding:utf-8 -*-

module Foolio
  class Handler
    include Foolio::Listener

    def initialize(handle)
      @handle = handle
    end

    def write(data)
      Foolio::UV.write(nil, @handle, data, callback{|status|
                         on_write_complete(status)})
    end

    def close
      $log.info "close #{@handle}"
      Foolio::UV.close(@handle, callback{
                         clear_callbacks
                       })
    end

    def on_write_complete(err); end
    def on_connect; end
    def on_close; end
  end

  class BlockHandler < Foolio::Handler
    def initialize(io, block)
      super(io)
      @block = block
    end

    def on_connect; end
    def on_close; end

    def on_read(data)
      @block.call data
    end
  end

  class Stream < Foolio::Handle
    def listen_handler(backlog, klass, *args)
      on_connect = callback do|status|
        client = init
        Foolio::UV.accept(@handle, client)
        $log.info "accept #{client}"
        handle = klass.new(client, *args)
        handle.on_connect

        on_read = callback {|data|
          if data then
            handle.on_read(data)
          else
            handle.close
          end
        }
        Foolio::UV.read_start(client, on_read)
      end
      Foolio::UV.listen(@handle, backlog, on_connect)
      self
    end

    def listen(backlog=5, &block)
      listen_handler(backlog, BlockHandler, block)
    end
  end

  class TCP < Stream
    def init
      Foolio::UV.tcp_init(@loop.loop)
    end
  end

  class Pipe < Stream
    def init
      Foolio::UV.pipe_init(@loop.loop, 0)
    end
  end

  class UDP < Foolio::Handle
    def bind(ip,port)
      @socket = Foolio::UV.ip4_addr(ip, port)
      Foolio::UV.udp_bind(@handle, @socket, 0)
      self
    end

    def start(&block)
      on_recv = callback do|data, addr, flags|
        ip   = Foolio::UV.ip_name addr
        port = Foolio::UV.port addr
        block.call(ip, port, data) if ip
      end
      Foolio::UV.udp_recv_start(@handle, on_recv)
    end

    def send(ip, port, data)
      on_send = proc {|_|}
      Foolio::UV.udp_send(nil, @handle, data, Foolio::UV.ip4_addr(ip, port), on_send)
    end
  end
end
