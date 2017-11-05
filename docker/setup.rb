#!/usr/bin/ruby

require 'yaml'
require 'fileutils'
require 'json'

setup = YAML::load_file('game.yml')

# Write an econfig file based on game.yml. We use to_json to quote-escape text.
econfig_custom_file = '/usr/local/etc/empire/econfig_custom'
File.open(econfig_custom_file, 'w') do |file|
  %w( privname privlog WORLD_X WORLD_Y ).each do |option|
    file.write("#{option} #{setup['config'][option].to_json}\n")
  end
end

# Process the custom econfig into a complete econfig
econfig_output_file = '/usr/local/etc/empire/econfig_output'
system("/usr/local/sbin/pconfig #{econfig_custom_file} > #{econfig_output_file}")
FileUtils.mv econfig_output_file, '/usr/local/etc/empire/econfig'

# Process that to generate game files, with no overwrite prompt, and no
# "All praise to POGO!" output.
system '/usr/local/sbin/files -f > /dev/null'

# Generate land.
newcap_script_file = '/usr/local/etc/empire/newcap_script'
fairland_args = %w( number_of_continents continent_size ).map do |arg|
  # TODO defaults for these would be nice, but see 'man fairland', it's complicated
  setup['fairland'][arg].to_s
end.join(" ")
system("/usr/local/sbin/fairland -s #{newcap_script_file} #{fairland_args} > /dev/null")

# TODO: run the server, use PTY to connect to it, log in as POGO/peter,
# change the POGO password to setup['players']['admin']['password'],
# set players' names to the other setup['players'] data,
# pipe "exec #{newcap_script_file}" to it, and stop it.
