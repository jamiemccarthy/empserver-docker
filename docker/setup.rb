#!/usr/bin/ruby

require 'safe_yaml'
require 'fileutils'
require 'json'

def load_setup
  SafeYAML::OPTIONS[:default_mode] = :safe
  $setup = YAML::load(STDIN.read)
  raise "error, input was not parsed as YAML data" unless $setup
  $econfig_custom_file      = '/usr/local/etc/empire/econfig_custom'
  $econfig_output_file      = '/usr/local/etc/empire/econfig_output'
  $econfig_destination_file = '/usr/local/etc/empire/econfig'
  $schedule_file            = '/usr/local/etc/empire/schedule'
  $newcap_script_file       = '/usr/local/etc/empire/newcap_script'
  $fairland_arg_names = %w(
    number_of_continents
    continent_size
    number_of_islands
    average_size_of_islands
    spike_percentage
    mountain_percentage
    continent_continent_minimum_distance
    continent_island_minimum_distance
  )
end

def write_econfig
  # Write an econfig file based on game.yml. We use to_json to quote-escape text.
  # TODO emit messages to STDOUT if parameters "data" or "port" are changed,
  # requiring the resulting volume to be invoked with different docker options.
  File.open($econfig_custom_file, 'w') do |file|
    %w( privname privlog WORLD_X WORLD_Y ).each do |option|
      file.write("#{option} #{$setup['config'][option].to_json}\n")
    end
  end
  raise "error, could not write custom econfig" unless File.size? $econfig_custom_file
end

def process_econfig
  # Process the custom econfig into a complete econfig.
  # TODO can pconfig fail and still write a file?
  system("/usr/local/sbin/pconfig #{$econfig_custom_file} > #{$econfig_output_file}")
  raise "error, could not process econfig" unless File.size? $econfig_output_file
  FileUtils.mv $econfig_output_file, $econfig_destination_file
  raise "error, could not copy econfig" unless File.size? $econfig_destination_file
end

def write_schedule
  # TODO allow options here
  File.open($schedule_file, 'w') do |file|
    file.write('every 10 minutes')
  end
  raise "error, could not write schedule" unless File.size? $schedule_file
  # TODO raise if this fails
  system("/usr/local/sbin/empsched")
end

def generate_game_files
  # Process the pconfig to generate game files, with no overwrite prompt, and no
  # "All praise to POGO!" output.
  system '/usr/local/sbin/files -f > /dev/null'
  raise "error, could not write game files" unless File.size? '/usr/local/var/empire/map'
end

def generate_land
  fairland_args = $fairland_arg_names.map do |arg|
    $setup['fairland'][arg].to_s
  end.join(" ")
  seed_arg = if $setup['fairland']['seed'].present?
               seed_arg = "-R #{$setup['fairland']['seed']}"
             else
               ""
             end
  system("/usr/local/sbin/fairland -q #{seed_arg} -s #{$newcap_script_file} #{fairland_args}")

  raise "error, running fairland failed" unless File.size? '/usr/local/var/empire/sector'
end

# TODO: run the server, use PTY to connect to it, log in as POGO/peter,
# change the POGO password to $setup['players']['admin']['password'],
# set players' names to the other $setup['players'] data,
# pipe "exec #{$newcap_script_file}" to it, and stop it.

########################################

load_setup
write_econfig
process_econfig
write_schedule
generate_game_files
generate_land
