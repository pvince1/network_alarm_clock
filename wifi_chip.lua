/* program.lua */
/* Transmits received TIME over UART every 2 seconds */

gpio.mode(3, gpio.OUTPUT)
gpio.mode(4, gpio.OUTPUT)
wifi.setmode(wifi.STATION)
wifi.sta.config("[WIFI NAME]","[WIFI PASSWORD]")

function handleSocketReceive(sck, c)
	gpio.write(3, gpio.HIGH)
	uart.write(0, c)
	sck:close()
end

function connect(t)
	if wifi.sta.status() == 5 then
		gpio.write(3, gpio.LOW)
		gpio.write(4, gpio.LOW)
		sk=net.createConnection(net.TCP, 0)
		sk:on("receive", handleSocketReceive)
		sk:connect(37,"198.111.152.100")
	else
		gpio.write(3, gpio.HIGH)
		gpio.write(4, gpio.HIGH)
	end
end

tmr.alarm(1, 2000, tmr.ALARM_AUTO, connect)