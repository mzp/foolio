# -*- encoding: utf-8 -*-
require File.expand_path('../lib/foolio/version', __FILE__)

Gem::Specification.new do |gem|
  gem.authors       = ["mzp"]
  gem.email         = ["mzpppp@gmail.com"]
  gem.description   = "Wow"
  gem.summary       = "Wow'
  gem.homepage      = ""

  gem.files         = `git ls-files`.split($\)
  gem.extensions    = ['ext/foolio/extconf.rb']
  gem.executables   = gem.files.grep(%r{^bin/}).map{ |f| File.basename(f) }
  gem.test_files    = gem.files.grep(%r{^(test|spec|features)/})
  gem.name          = "foolio"
  gem.require_paths = ["lib", "ext"]
  gem.version       = Foolio::VERSION
end
