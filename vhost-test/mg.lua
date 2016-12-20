local mg      = require "moongen"
local memory  = require "memory"
local device  = require "device"
local stats   = require "stats"
local hist    = require "histogram"
local timer   = require "timer"
local barrier = require "barrier"
local table   = require "table"
local math    = require "math"

function configure(parser)
	parser:description("Packet loss graph.")
	parser:option("--txport", "Device to transmit/receive from."):convert(tonumber)
	parser:option("--rxport", "Device to transmit/receive from."):convert(tonumber)
	parser:option("--dst", "Frame's destination hardware address")
	parser:option("--duration", "Device to transmit/receive from."):default(20):convert(tonumber)
	parser:option("--frameSize", "Ethernet frame size."):default(64):convert(tonumber)
	parser:option("--maxRateInterval", "Max % rate interval between measures"):default(5):convert(tonumber)
	parser:option("--minRateInterval", "Min % rate interval between measures"):default(0.1):convert(tonumber)
	parser:option("--targetDropRateRatio", "Target drop rate ratio between measures"):default(2):convert(tonumber)
	parser:option("--out", "Output file"):default("vhost_mg_"..os.date("%F_%H-%M")..".txt")
end

function getHeaderString(file)
	return string.format("%10s\t%10s\t%12s\t%12s\t%12s\t%12s\t%12s\t%12s\t%12s\t%12s", 
	"size(B)", "Time(s)", "Rate(Mbps)", "Tx(pkt)", "Rx(pkt)", "DropRate(%)", "Tx(Mpps)", "Rx(Mpps)", "Tx(Gbps)", "Rx(Gbps)")
end

function getResultString(result, file)
	return string.format("%10d\t%10d\t%12f\t%12d\t%12d\t%12f\t%12f\t%12f\t%12f\t%12f", 
	result.frameSize, result.duration, result.rate, result.tx, result.rx, result.dropRate, result.txMpps, result.rxMpps, result.txRate, result.rxRate)
end

function runMeasure(txDev, rxDev, frameSize, duration, rate)
	local bar = barrier.new(2)
	local result = {}
	txDev:getTxQueue(0):setRate(rate * frameSize / (frameSize+20))
	local stask = mg.startTask("loadSlave", txDev:getTxQueue(0), frameSize, duration, bar)
	local rtask = mg.startTask("counterSlave", rxDev:getRxQueue(0), frameSize, duration, bar)
	result.frameSize = frameSize
	result.duration = duration
	result.tx = stask:wait()
	result.rx = rtask:wait()
	result.rate = rate
	result.dropRate = (result.tx - result.rx)/result.tx * 100
	result.txMpps = (result.tx) / duration / 1000000
	result.rxMpps = (result.rx) / duration / 1000000
	result.txRate = (result.tx * frameSize * 8) / duration / 1000000000
	result.rxRate = (result.rx * frameSize * 8) / duration / 1000000000
	
	print(getResultString(result))
	return result
end

function master(args)
	local txDev = device.config({port = args.txport, rxQueues = 1, txQueues = 1})
	local rxDev = device.config({port = args.rxport, rxQueues = 1, txQueues = 1})
	local frameSize = args.frameSize
	local duration = args.duration
	device.waitForLinks()
	
	local maxLinkRate = txDev:getLinkStatus().speed
	local results = {}
	
	-- Warming up
	print ("Output file is: "..args.out)
	print ("Start Warm-Up")
	runMeasure(txDev, rxDev, frameSize, 1, maxLinkRate)
	runMeasure(txDev, rxDev, frameSize, 1, 1000)
	print ("Stop Warm-Up")
	
	local sortFunction = function(a,b)
		return a.rate < b.rate
	end
	
	local file = io.open(args.out, "w")
	io.output(file)
	io.write(getHeaderString().."\n")
	io.close(file)
	
	print ("Start Measures")
	print(getHeaderString())
	table.insert(results, runMeasure(txDev, rxDev, frameSize, duration, maxLinkRate))
	table.insert(results, runMeasure(txDev, rxDev, frameSize, duration, maxLinkRate/2))
	table.insert(results, runMeasure(txDev, rxDev, frameSize, duration, 100))
	table.sort(results, sortFunction)
	
	while mg.running() do
		local chosen = nil
		local chosenMeaning = 0
		for i, r1 in ipairs(results) do
			local r2 = results[i+1]
			if r2 == nil then
				break
			end
			local rateDiff = r2.rate - r1.rate
			local dropRateRatio = (r2.dropRate + 0.00000001)/(r1.dropRate + 0.00000001)
			if r2.dropRate < r1.dropRate then  
				dropRateRatio = (r1.dropRate+ 0.00000001)/(r2.dropRate + 0.00000001)
			end
			-- Meaning rates next measures
			-- The idea is to compute interesting results first and get more picky later.
			local meaning = rateDiff / ((args.maxRateInterval * maxLinkRate) / 100 ) + math.log(dropRateRatio / args.targetDropRateRatio)/math.log(2)
			if rateDiff > (args.maxRateInterval * maxLinkRate) / 100
				or (dropRateRatio > args.targetDropRateRatio and (rateDiff) > (args.minRateInterval * maxLinkRate) / 100) then
					if meaning > chosenMeaning then
						chosenMeaning = meaning
						chosen = i
					end
			end
		end
		if (chosen == nil) then
			break
		end
		local nextRate = (results[chosen+1].rate + results[chosen].rate)/2
		table.insert(results, runMeasure(txDev, rxDev, frameSize, duration, nextRate))
		table.sort(results, sortFunction)
		
		local file = io.open(args.out, "w")
		io.output(file)
		io.write(getHeaderString().."\n")
		for i, r1 in ipairs(results) do
			io.write(getResultString(r1).."\n")
		end
		io.close(file)
	end
	
	
end

function loadSlave(queue, frameSize, duration, bar)
    bar:wait()

	local mem = memory.createMemPool(function(buf)
		buf:getEthernetPacket():fill{
			ethSrc = queue,
			ethDst = ETH_DST,
			ethType = 0x1234
		}
	end)

    local bufs = mem:bufArray()
    local timer = timer:new(duration)
	local total = 0;
	while timer:running() do
		bufs:alloc(frameSize)
		total = total + queue:send(bufs)
	end
	return total
end

function counterSlave(queue, frameSize, duration, bar)
    local bufs = memory.bufArray()
	local total = 0;
    bar:wait()

    local timer = timer:new(duration + 1)
    while timer:running() do
		total = total + queue:tryRecv(bufs, 1000)
		bufs:freeAll()
    end
    return total
end

