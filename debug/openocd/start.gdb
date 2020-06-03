# reload the binary
define reset
	print "Resetting target"
	monitor reset halt
	monitor bp 0x80200000 1 hw
	monitor resume
	monitor wait_halt 5000
	monitor rbp 0x80200000
	load
end

# enable history
set history filename ~/.gdb_history
set history save

# some behavioural settings
set print pretty on
set pagination off
set confirm off

# connect
target extended-remote localhost:3333
monitor gdb_breakpoint_override hard

# load application for the first time
reset
