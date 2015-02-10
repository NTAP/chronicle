#!/usr/bin/python

import sys, os, subprocess, shlex
from optparse import OptionParser
import struct
import hashlib
import threading
import logging, time
import re

class AES_GPG_Decryptor(threading.Thread):
	def __init__(self, inputFile):
		threading.Thread.__init__(self)
		self.inputFile = inputFile

	def run(self):
		if self.inputFile.endswith(".gpg") == False:
			return
		(self.inputDir,fileName) = os.path.split(self.inputFile)
		decryptedFile = re.sub(r'.gpg$', '', fileName)
		outputFile = os.path.join(outputDir, decryptedFile)
		if os.path.exists(outputFile):
			print 'not overwriting {}'.format(outputFile)
			return
		key256 = re.sub(r'\'|\"', 'X', hashlib.sha256(passphrase).digest()) 
		command = 'gpg -d --passphrase ' + key256 + ' -o ' + outputFile + ' ' + self.inputFile
		print command
		try:
			subprocess.check_call(shlex.split(command))
		except subprocess.CalledProcessError as e:
			logging.debug('ERROR: [{}] {} {} {}'.format(time.strftime(
				"%H:%M:%S %m:%d:%Y", time.localtime()),
				e.cmd, e.returncode, e.output))

def parseCommandLine():
	parser = OptionParser()
	parser.add_option("-i", "--inputDir", dest="inputDir", type="string",
		help="directory where encrypted DataSeries traces are stored", 
		metavar="DIR")
	parser.add_option("-o", "--outputDir", dest="outputDir", type="string",
		help="directory where decrypted traces get stored (REQUIRED)", 
		metavar="DIR")
	parser.add_option("-f", "--file", dest="fileName", type="string",
		help="encrypted file", metavar="FILE")
	parser.add_option("-p", "--passphrase", dest="passphrase", type="string", 
		help="symmetric decryption using the passphrase (REQUIRED)",
		metavar="STRING")
	parser.add_option("-t", "--threads", dest="threads", type="int")

	(options, args) = parser.parse_args()
	if (options.inputDir == None and options.fileName == None) or options.passphrase == None or options.outputDir == None:
		parser.print_help()
		exit(1)
	if options.inputDir != None and options.fileName != None:
		print 'ERROR: only one of -i or -f options can be used'
		exit(1)
	return (options.inputDir, options.outputDir, options.fileName, 
        options.passphrase, options.threads)

# process command line arguments
(inputDir, outputDir, fileName, passphrase, threads) = parseCommandLine()
if ((inputDir != None and os.path.exists(inputDir)) or os.path.exists(outputDir)):
	print 'inputDir:{}   outputDir:{}   passphrase:{}'.format(inputDir, outputDir, passphrase)
	print '================================================='
elif inputDir != None:
	print 'ERROR: input or output directory does not exist!'
	exit(1)
if fileName != None and os.path.exists(fileName):
	print 'fileName:{}   passphrase:{}'.format(fileName, passphrase)
	print '================================================='
elif fileName != None:
	print 'ERROR: file name does not exist!'
	exit(1)
logging.basicConfig(filename=os.path.join(outputDir, 'decryption.log'),level=logging.DEBUG)
if threads == None:
	threads = 1

if inputDir != None:
	# decrypt all files in the inputDir
	for f in os.listdir(inputDir):
		while threading.active_count() > threads:
			time.sleep(2)
		try:
			t = AES_GPG_Decryptor(os.path.join(inputDir, f))
			t.start()
		except:
			logging.debug("ERROR: [{}] Unable to start the decryption thread for {}".format(time.strftime("%H:%M:%S %m:%d:%Y", time.localtime()), os.path.join(inputDir, f)))
elif fileName != None:
	# decrypt the input file
	try:
		t = AES_GPG_Decryptor(fileName)
		t.start()
	except:
		logging.debug("ERROR: [{}] Unable to start the decryption thread for {}".format(time.strftime("%H:%M:%S %m:%d:%Y", time.localtime()), os.path.join(inputDir, f)))

while threading.active_count() > 1:
	time.sleep(5)
