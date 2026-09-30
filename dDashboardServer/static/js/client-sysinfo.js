/**
 * Client system info (client_sysinfo): the card on an account's page (#sysinfoCard) with every description the
 * account's client sent at login, and the Client System Info page (#sysinfoPage) with the spread across players.
 * Everything here is as reported by the client, which often reports compatibility values rather than the real
 * hardware; each field is shown with what it is worth (the caveats the API sends).
 */
(function () {
	'use strict';

	function kb(n) { return n === null || n === undefined ? '-' : fmt.bytes(Number(n) * 1024); }
	function bytes(n) { return n === null || n === undefined ? '-' : fmt.bytes(n); }
	function hex(n) { return '0x' + Number(n).toString(16); }
	function caveat(caveats, key) { return caveats[key] ? '<div class="small text-body-secondary">' + esc(caveats[key]) + '</div>' : ''; }

	// Every field of one row: [label, value (html), caveat key]
	function fields(r, showsIp) {
		var m = r.memory;
		var list = [
			['Client build (clientOS)', esc(r.client_os) + ' (' + esc(r.client_os_name) + ')', 'clientOs'],
			['Memory text (memoryStats), as sent', '<code class="text-break">' + esc(r.memory_stats) + '</code>' + (m.complete ? '' : ' <span class="badge text-bg-secondary">not fully read</span>'), 'memoryStats'],
			['Physical memory: total / free', kb(m.total_phys_kb) + ' / ' + kb(m.avail_phys_kb) + (m.memory_load_percent === null ? '' : ' (' + esc(m.memory_load_percent) + '% in use)'), ''],
			['Commit limit (pfile): total / free', kb(m.total_pagefile_kb) + ' / ' + kb(m.avail_pagefile_kb), ''],
			['Client address space (vmem): total / free', kb(m.total_virtual_kb) + ' / ' + kb(m.avail_virtual_kb), ''],
			['Client process: working set / private (peak)', bytes(m.working_set_bytes) + ' / ' + bytes(m.pagefile_usage_bytes) + ' (' + bytes(m.peak_working_set_bytes) + ' / ' + bytes(m.peak_pagefile_usage_bytes) + ')', ''],
			['Video card (videoCard)', esc(r.video_card || '(empty)'), 'videoCard'],
			['Processors (numberOfProcessors)', esc(r.number_of_processors), 'numberOfProcessors'],
			['Processor type (processorType)', esc(r.processor_type), 'processorType'],
			['Processor level (processorLevel)', esc(r.processor_level), 'processorLevel'],
			['Processor revision (processorRevision)', esc(r.processor_revision) + ' (' + hex(r.processor_revision) + ': model ' + esc(r.processor_model) + ', stepping ' + esc(r.processor_stepping) + ')', 'processorRevision'],
			['Windows version (major.minor.build)', esc(r.os_version) + ' <span class="text-body-secondary">' + esc(r.os_label) + '</span>', 'osVersion'],
			['Platform ID (platformID)', esc(r.os_platform_id), 'platformId'],
			['Version info size (osVersionInfoSize)', esc(r.os_version_info_size), 'osVersionInfoSize']
		];
		if (showsIp) list.unshift(['Address', r.ip ? '<code>' + esc(r.ip) + '</code>' : '<span class="text-body-secondary">not kept</span>', 'ip']);
		return list;
	}

	function accountCard(card) {
		var accountId = card.dataset.account;
		api.get('/api/accounts/' + accountId + '/client_sysinfo').then(function (d) {
			if (!d.success) return;
			document.getElementById('sysinfoCount').textContent = d.rows.length;
			var list = document.getElementById('sysinfoList');
			if (!d.rows.length) {
				list.innerHTML = '<p class="text-body-secondary mb-0">Nothing on record: the client sends this at each game login (kept for log_client_sysinfo_days).</p>';
				return;
			}
			list.innerHTML = d.rows.map(function (r, i) {
				var summary = '<span class="fw-semibold">' + esc(fmt.unix(r.last_seen)) + '</span>' +
					(r.first_seen !== r.last_seen ? ' <span class="text-body-secondary">since ' + esc(fmt.unix(r.first_seen)) + '</span>' : '') +
					' · ' + esc(r.logins) + ' login' + (r.logins === 1 ? '' : 's') +
					' · reports Windows ' + esc(r.os_version) + ' · ' + esc(r.video_card || 'no video card') + ' · ' + esc(r.number_of_processors) + ' CPUs · ' + kb(r.memory_total_kb) +
					(d.showsIp && r.ip ? ' · <code>' + esc(r.ip) + '</code>' : '');
				var rows = fields(r, d.showsIp).map(function (f) {
					return '<tr><th class="fw-normal text-nowrap">' + esc(f[0]) + '</th><td>' + f[1] + caveat(d.caveats, f[2]) + '</td></tr>';
				}).join('');
				return '<details class="mb-2"' + (i === 0 ? ' open' : '') + '><summary>' + summary + '</summary>' +
					'<div class="table-responsive"><table class="table table-sm mb-0"><tbody>' + rows + '</tbody></table></div></details>';
			}).join('');
		}).catch(function () {});
	}

	function spreadPage() {
		api.get('/api/client_sysinfo/spread').then(function (d) {
			if (!d.success) return;
			var s = d.spread;
			document.getElementById('sysinfoAccounts').textContent = s.accounts;
			[['os', 'osVersion'], ['video', 'videoCard'], ['memory', 'memoryStats'], ['processors', 'numberOfProcessors'], ['clientOs', 'clientOs']].forEach(function (pair) {
				var el = document.getElementById('spread-' + pair[0]);
				if (!el) return;
				var rows = s[pair[0]];
				var html = rows.map(function (e) {
					var pct = s.accounts ? Math.round(e.count * 1000 / s.accounts) / 10 : 0;
					return '<tr><td>' + esc(e.label) + '</td><td class="text-end text-nowrap">' + esc(e.count) + '</td>' +
						'<td style="width:40%"><div class="progress" role="progressbar" aria-label="' + esc(e.label) + '" aria-valuenow="' + pct + '" aria-valuemin="0" aria-valuemax="100">' +
						'<div class="progress-bar" style="width:' + pct + '%"></div></div></td><td class="text-end text-nowrap small">' + pct + '%</td></tr>';
				}).join('') || '<tr><td class="text-body-secondary">Nothing reported yet.</td></tr>';
				el.querySelector('tbody').innerHTML = html;
				var note = el.querySelector('[data-caveat]');
				if (note) note.textContent = d.caveats[pair[1]] || '';
			});
		}).catch(function () {});
	}

	var card = document.getElementById('sysinfoCard');
	if (card) accountCard(card);
	if (document.getElementById('sysinfoPage')) spreadPage();
})();
