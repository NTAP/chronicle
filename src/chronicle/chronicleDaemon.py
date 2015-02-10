#!/usr/bin/python

import sys, subprocess, shlex
from optparse import OptionParser
import time
import threading
import multiprocessing
import os, logging

class CliThread(threading.Thread):
	def __init__(self, event):
		threading.Thread.__init__(self)
		self.done = False
		self.shutdownEvent = event;
	
	def run(self):
		self.chronicleCLI()
	
	def chronicleCLI(self):
		global chronicle
		while not self.done and not self.shutdownEvent.is_set():
			command = sys.stdin.readline()
			if command == 'QUIT\n':
				self.shutdownEvent.set()
			if command == 'RESTART\n':
				command = 'QUIT\n'
			if not self.done:
				chronicle.stdin.write(command)

def cliNotify(cli, seconds):
	global chronicle
	print "Chronicle ran for {} seconds".format(seconds)
	chronicle.stdin.write('QUIT\n')
	cli.done = True

def parseCommandLine():
	def getNics(option, opt_str, value, parser):
		setattr(parser.values, option.dest, value.split(','))
	
	parser = OptionParser()
	parser.add_option("-i", "--interface", action="callback", callback=getNics,
		type="string", dest="nics", help="NIC1,NIC2,... [REQUIRED]")
	parser.add_option("-o", "--outputDir", dest="outputDir", type="string",
		help="directory where traces are stored", 
		metavar="DIR")
	parser.add_option("-O", "--outputType", dest="outputType", type="string",
		help="Dx (for x DataSeries writer modules); Px (for x Pcap writer modules)")
	parser.add_option("-m", "--minutes", dest="minutes", type="int",
		help="minutes to run", metavar="INT")
	parser.add_option("-r", "--hours", dest="hours", type="int",
		help="hours to run", metavar="INT")
	parser.add_option("-d", "--days", dest="days", type="int", 
		help="days to run", metavar="INT")
	parser.add_option("-p", "--pipeline", dest="pipeline", type="string",
		help="nx (for x NFS pipelines); px (for x Pcap pipelines) [REQUIRED]")
	parser.add_option("-l", "--logDir", dest="logDir", type="string",
		help="Chronicle daemon log directory [REQUIRED]")
	parser.add_option("-X", action="store_true", dest="exclude", default=False,
		help="exclude IP and data checksum")
	parser.add_option("-a", action="store_true", dest="analytics", default=False,
		help="enable inline analytics")
	
	(options, args) = parser.parse_args()
	if options.nics == None or options.pipeline == None or options.logDir == None or (options.pipeline.find('n') != 0 and  options.pipeline.find('p') != 0) or (options.outputType != None and options.outputType.find('D') != 0 and  options.outputType.find('P') != 0):
		parser.print_help()
		exit(1)
	if options.minutes == None:
		options.minutes = 0
	if options.hours == None:
		options.hours = 0
	if options.days == None:
		options.days = 0
	command = './chronicle_netmap '
	for nic in options.nics:
		command += '-i' + nic + ' '
	command += '-' + options.pipeline
	if options.outputDir != None:
		command += ' -o ' + options.outputDir
	if options.outputType != None:
		command += ' -' + options.outputType
	if options.exclude:
		command += ' -X'
	if options.analytics:
		command += ' -a'
	seconds = options.minutes*60 + options.hours*3600 + options.days*24*3600
	return (command, seconds, options.logDir)


(chronicleCommand, seconds, logDir) = parseCommandLine()
logging.basicConfig(filename=os.path.join(logDir, 'chronicle.log'),level=logging.DEBUG)
currentTime = time.time()
endTime = currentTime + seconds
logging.info('running Chronicle for {} seconds starting at {}'.format(seconds, 
	time.strftime("%H:%M:%S %m/%d/%Y", time.localtime())))
logging.info(chronicleCommand)
run = True
trial = 1
shutdownEvent = multiprocessing.Event()
cli = CliThread(shutdownEvent)
cli.start()
print "Chronicle will be running for {} seconds".format(seconds)
timer = threading.Timer(seconds, cliNotify, (cli, seconds))
timer.start()
while run:
	chronicle = subprocess.Popen(shlex.split(chronicleCommand), stdin=subprocess.PIPE)
	chronicle.wait()
	shutdownEvent.wait(1)
	if time.time() > endTime or shutdownEvent.is_set():
		run = False
		timer.cancel()
		cli.join()
	else:
		trial += 1
		logging.warning('restarting Chronicle at {} (# {})'.format(
			time.strftime("%H:%M:%S %m/%d/%Y", time.localtime()), trial))
logging.info('Chronicle terminated at {} (# {})'.format(
	time.strftime("%H:%M:%S %m/%d/%Y", time.localtime()), trial))
timer.join()

