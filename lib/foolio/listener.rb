# -*- mode:ruby; coding:utf-8 -*-

module Foolio
  module Listener
    def callback(&f)
      const_name = "Callback_#{f.object_id}"
      unless self.class.const_defined?(const_name)
        callbacks << const_name
        self.class.const_set(const_name, f)
      end
      self.class.const_get(const_name)
    end

    private
    def callbacks
      @callbacks ||= Set.new
    end

    def clear_callbacks
      callbacks.each do |name|
        self.class.send(:remove_const, name)
      end
      callbacks.clear
    end
  end
end
