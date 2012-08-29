# -*- mode:ruby; coding:utf-8 -*-

module Foolio
  class Loop
    include Foolio::Listener
    attr_reader :loop
    class << self
      def default
        self.new Foolio::UV.default_loop
      end

      def create
        self.new Foolio::UV.loop_new
      end
    end

    def initialize(loop)
      @loop = loop

      timer.start(1) do
        if @stop
          Foolio::UV.close_all(@loop, callback{
                                 clear_callbacks
                               })
        end
      end
    end

    def idle
      Foolio::Idle.new(self, Foolio::UV.idle_init(@loop))
    end

    def timer
      Foolio::Timer.new(self, Foolio::UV.timer_init(@loop))
    end

    def tcp(ip, port)
      handle = Foolio::UV.tcp_init(@loop)
      @socket = Foolio::UV.ip4_addr(ip, port)
      Foolio::UV.tcp_bind(handle, @socket)
      Foolio::TCP.new self, handle
    end

    def udp
      handle = Foolio::UV.udp_init(@loop)
      Foolio::UDP.new self, handle
    end

    def unix(path, ipc = 0)
      handle = Foolio::UV.pipe_init(@loop, ipc)
      Foolio::UV.pipe_bind(handle, path)
      Foolio::Pipe.new self, handle
    end

    def file_stat(path, &block)
      cb = callback{|event, _|
        block[path, event]
      }
      Foolio::UV.fs_event_init(@loop, path, cb, 0)
    end

    def run
      Foolio::UV.run @loop
    end

    def alive?
      not @stop
    end

    def stop
      @stop = true
    end

    def force_stop
      @stop = true
      Foolio::UV.loop_delete @loop
    end
  end
end
