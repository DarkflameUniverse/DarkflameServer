/**
 * The zone and spawn point pickers for rescuing a character (Online Players, a character's page).
 * Both lists come from the server: every zone in the client's ZoneTable, and for the picked zone its named spawn points
 * (from the zone's own files, the spots rocket launchers land players on). An empty spawn point uses the zone's default.
 */
(function () {
	'use strict';

	var zonesLoad = null, spawnCache = {};

	function zones() {
		if (!zonesLoad) zonesLoad = api.get('/api/zones').then(function (d) { return d.success ? d.zones : []; }).catch(function () { return []; });
		return zonesLoad;
	}

	function spawnPoints(zone) {
		if (!spawnCache[zone]) spawnCache[zone] = api.get('/api/zones/' + zone + '/spawn_points').then(function (d) {
			return d.success ? d.spawnPoints : null;
		}).catch(function () { return null; });
		return spawnCache[zone];
	}

	/**
	 * Fill zoneSelect and spawnSelect. options.zone: the zone to start on; options.here: {zone, x, z} of where the player
	 * is, so spawn points in that zone show how far away they are.
	 */
	window.RescuePicker = function (zoneSelect, spawnSelect, options) {
		options = options || {};
		var help = document.getElementById(spawnSelect.id + 'Help');

		function fillSpawns() {
			var zone = parseInt(zoneSelect.value, 10);
			spawnSelect.disabled = true;
			spawnSelect.innerHTML = '<option value="">Loading&hellip;</option>';
			return spawnPoints(zone).then(function (points) {
				if (parseInt(zoneSelect.value, 10) !== zone) return; // picked another zone meanwhile
				var here = options.here && options.here.zone === zone ? options.here : null;
				spawnSelect.innerHTML = '<option value="">The zone\'s default spawn</option>' + (points || []).map(function (p) {
					var away = here ? ' · ' + Math.round(Math.hypot(p.x - here.x, p.z - here.z)) + ' away' : '';
					return '<option value="' + esc(p.name) + '">' + esc(p.name) + ' (' + [p.x, p.y, p.z].map(function (v) { return Math.round(v); }).join(', ') + away + ')</option>';
				}).join('');
				spawnSelect.disabled = !points || !points.length;
				if (help) help.textContent = points === null ? 'The server can\'t read this zone\'s files (client_location), so only its default spawn is offered.'
					: !points.length ? 'This zone has no named spawn points; they land on its default spawn.'
					: points.length + ' named spawn points, the same spots rocket launchers and doors land players on.';
			});
		}

		zones().then(function (list) {
			// Starts on the zone they're in (or were last in); the list is the server's, in zone ID order
			var start = options.zone && list.some(function (z) { return z.id === options.zone; }) ? options.zone : (list[0] || {}).id;
			zoneSelect.innerHTML = list.map(function (z) {
				return '<option value="' + esc(z.id) + '"' + (z.id === start ? ' selected' : '') + '>' + esc(z.name) + ' (' + esc(z.id) + ')</option>';
			}).join('') || '<option value="">No zones (the server needs the client\'s files)</option>';
			if (list.length) fillSpawns();
		});
		zoneSelect.onchange = fillSpawns; // replaces the last picker's, when the dialog is opened again

		return {
			zone: function () { return parseInt(zoneSelect.value, 10); },
			spawnPoint: function () { return spawnSelect.value; },
			label: function () {
				var zone = zoneSelect.options[zoneSelect.selectedIndex];
				return (zone ? zone.textContent : 'zone ' + zoneSelect.value) + (spawnSelect.value ? ' at ' + spawnSelect.value : '');
			}
		};
	};
})();
