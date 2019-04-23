
state.var {
	pocMap = state.map()
}

function registerNewPoc(name, devices)
	-- only creator can register a new poc
	assert(system.getCreator() == system.getOrigin(), "sender cannot register a new poc")

	pocMap[name] = {
		name = name,
		devices = devices,
		last_tx_hash = "",
		total_tx_count = 0,
	}
end

function updateDevices(name, devices)
	-- only creator can update device info
	assert(system.getCreator() == system.getOrigin(), "sender cannot update device info")

	pocMap[name]["devices"] = devices
end

function updateTxInfo(name, last_tx_hash, tx_count)
	-- only creator can update tx info
	assert(system.getCreator() == system.getOrigin(), "sender cannot update tx info")

	pocMap[name]["last_tx_hash"] = last_tx_hash
	pocMap[name]["total_tx_count"] = tx_count
end

function getPocInfo(name)
	return pocMap[name]
end

abi.register(registerNewPoc, updateDevices, updateTxInfo, getPocInfo)
