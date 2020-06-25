local zeroconf = require("plugin.zeroconf")
local json = require("json")
local widget = require('widget')

display.setStatusBar( display.HiddenStatusBar )


local function onRowRender( event )
	local row = event.row
	local w,h = row.contentWidth, row.contentHeight
	local params = row.params

	t = display.newText( {
		parent = row,
		y = h*0.30,
		x = h*0.5,
		text = params.name,
		font = native.systemFontBold,
		align = "left",
	} )
	t:setFillColor( {0,0,} )
	t:translate( t.width*0.5, 0 )

	t = display.newText( {
		parent = row,
		y = h*0.7,
		x = h*0.5,
		text = params.address,
		align = "left",
	} )
	t:setFillColor( {0,0,} )
	t:translate( t.width*0.5, 0 )

end

local achievementsTable = widget.newTableView({
	onRowRender = onRowRender
})




function listener( event )
	print( json.prettify(event) )
	if event.phase == 'found' and not event.isError then
		addr = ""
		if event.addresses and #event.addresses > 0 then
			if #addr > 0 then
				addr = addr .. "   "
			end
			addr = addr .. event.addresses[1] .. ":" .. event.port
		end
		if event.hostname and #event.hostname>0 then
			if #addr > 0 then
				addr = addr .. "   "
			end
			addr = addr .. event.hostname .. ":" .. event.port
		end
		achievementsTable:insertRow( {
			rowHeight = 35,
			params = {
				name = event.serviceName,
				address = addr ,
			}
	} )
	elseif event.phase == 'lost' then
		for i=1,achievementsTable:getNumRows( ) do
			local row = achievementsTable:getRowAtIndex( i )
			if row and row.params.name == event.serviceName then
				achievementsTable:deleteRow( i )
				break;
			end
		end
	end
end

zeroconf.init( listener )

local name = system.getInfo("name") .. " (".. system.getInfo("platformName") .. " " .. system.getInfo("platformVersion") .. ")"

local service = zeroconf.publish{ port=2929, name=name }
local browser = zeroconf.browse{}

-- local browser = zeroconf.browse{type='_airplay._tcp'}

