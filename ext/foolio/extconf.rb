require "mkmf"

$CFLAGS << " -std=gnu99 -fPIC"

$DLDFLAGS << " -fPIC"
$warnflags.gsub!("-Wdeclaration-after-statement","")

if RUBY_PLATFORM =~ /darwin/
  $DLDFLAGS << " -framework CoreServices"
end

libuv_dir = File.expand_path("../../libuv", __FILE__)
foolio_dir = File.expand_path("../", __FILE__)

Dir.chdir(libuv_dir) do 
  system "make"
end

Dir.chdir(foolio_dir) do
  require 'fileutils'
  FileUtils.cp "#{libuv_dir}/uv.a", "#{foolio_dir}/libuv.a"
end

dir_config "uv", "#{libuv_dir}/include", foolio_dir
have_library "ws2_32"
have_library "psapi"
have_library "iphlpapi"
have_library "uv"

create_makefile "foolio/foolio_ext"
