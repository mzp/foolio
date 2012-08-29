# -*- mode:ruby; coding:utf-8 -*-

module Foolio
  class Handle
    def initialize(loop, handle)
      @loop = loop
      @handle = handle
    end

    def callback(&block)
      @loop.callback &block
    end
  end

  class Idle < Foolio::Handle
    def start(&block)
      Foolio::UV.idle_start(@handle, callback(&block))
    end
  end

  class Timer < Foolio::Handle
    def start(interval, &block)
      sec    = (interval * 1000).to_i
      Foolio::UV.timer_start(@handle, callback(&block), sec, sec)
    end
  end
end
