/**
 * Client system info (client_sysinfo): the table on an account's page (#sysinfoCard) with every description the
 * account's client sent at login, and the Client System Info page (#sysinfoPage) with the spread across players and
 * every report browsable (#sysinfoTable). Everything here is as reported by the client, which often reports
 * compatibility values rather than the real hardware: values that can't be trusted are marked, and their tooltip says
 * what they are worth (the caveats and trust levels the API sends).
 */
(function () {
	'use strict';

	var caveats = {}, trust = {}, showsIp = false;

	function kb(n) { return n === null || n === undefined ? '-' : fmt.bytes(Number(n) * 1024); }
	function bytes(n) { return n === null || n === undefined ? '-' : fmt.bytes(n); }
	function hex(n) { return '0x' + Number(n).toString(16); }

	// A value with its caveat as a tooltip: unreliable values get a warning mark, approximate ones a dotted underline
	function tip(key, html) {
		var level = trust[key], text = caveats[key];
		if (!text) return html;
		var mark = level === 'unreliable' ? ' <span class="text-warning" aria-hidden="true">&#9888;</span>' : '';
		var label = level === 'unreliable' ? 'Unreliable: ' : level === 'approximate' ? 'Approximate: ' : '';
		return '<span class="sysinfo-tip sysinfo-' + esc(level || 'note') + '" tabindex="0" data-bs-toggle="tooltip" data-bs-title="' + esc(label + text) + '">' +
			html + mark + '</span>';
	}

	function tooltips(root) {
		if (!window.bootstrap) return;
		root.querySelectorAll('[data-bs-toggle="tooltip"]').forEach(function (el) { bootstrap.Tooltip.getOrCreateInstance(el); });
	}

	// Every field of one row: [label, value (html), caveat key]
	function fields(r) {
		var m = r.memory;
		var list = [
			['Client build (clientOS)', esc(r.client_os) + ' (' + esc(r.client_os_name) + ')', 'clientOs'],
			['Memory text (memoryStats), as sent', '<code class="text-break">' + esc(r.memory_stats) + '</code>' + (m.complete ? '' : ' <span class="badge text-bg-secondary">not fully read</span>'), 'memoryStats'],
			['Physical memory: total / free', kb(m.total_phys_kb) + ' / ' + kb(m.avail_phys_kb) + (m.memory_load_percent === null ? '' : ' (' + esc(m.memory_load_percent) + '% in use)'), 'memoryStats'],
			['Commit limit (pfile): total / free', kb(m.total_pagefile_kb) + ' / ' + kb(m.avail_pagefile_kb), 'memoryStats'],
			['Client address space (vmem): total / free', kb(m.total_virtual_kb) + ' / ' + kb(m.avail_virtual_kb), 'memoryStats'],
			['Client process: working set / private (peak)', bytes(m.working_set_bytes) + ' / ' + bytes(m.pagefile_usage_bytes) + ' (' + bytes(m.peak_working_set_bytes) + ' / ' + bytes(m.peak_pagefile_usage_bytes) + ')', 'memoryStats'],
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

	function details(r) {
		return '<div class="table-responsive"><table class="table table-sm mb-0"><tbody>' + fields(r).map(function (f) {
			return '<tr><th class="fw-normal text-nowrap">' + esc(f[0]) + '</th><td>' + tip(f[2], f[1]) + '</td></tr>';
		}).join('') + '</tbody></table></div>';
	}

	// The columns, in the order the server sorts by (last seen, account, logins, Windows, video card, processors, memory,
	// client build, first seen)
	function columns(withAccount) {
		return [
			{ data: 'last_seen', render: function (d, t) { return t === 'display' ? esc(fmt.unix(d)) : d; } },
			{ data: 'account_name', visible: withAccount, render: function (d, t, r) {
				return '<a href="/accounts/' + encodeURIComponent(r.account_id) + '">' + esc(d || ('#' + r.account_id)) + '</a>';
			} },
			{ data: 'logins', className: 'text-end' },
			{ data: 'os_version', render: function (d, t, r) { return tip('osVersion', esc(d)) + '<div class="small text-body-secondary">' + esc(r.os_label) + '</div>'; } },
			{ data: 'video_card', render: function (d) { return tip('videoCard', esc(d || '(empty)')); } },
			{ data: 'number_of_processors', className: 'text-end', render: function (d) { return tip('numberOfProcessors', esc(d)); } },
			{ data: 'memory_total_kb', className: 'text-end text-nowrap', render: function (d) { return tip('memoryStats', d ? kb(d) : 'Not read'); } },
			{ data: 'client_os_name', render: function (d) { return tip('clientOs', esc(d)); } },
			{ data: 'first_seen', render: function (d, t) { return t === 'display' ? esc(fmt.unix(d)) : d; } }
		];
	}

	// A browsable table of reports; clicking a row opens every field
	function reportTable(selector, extra, withAccount) {
		var el = document.querySelector(selector);
		var table = serverTable(selector, '/api/tables/client_sysinfo', columns(withAccount), {
			extra: extra,
			dataTable: { order: [[0, 'desc']], language: { searchPlaceholder: withAccount ? 'Account or video card' : 'Video card' } }
		});
		table.on('xhr', function (e, settings, json) {
			if (!json) return;
			caveats = json.caveats || caveats;
			trust = json.trust || trust;
			showsIp = !!json.showsIp;
		});
		table.on('draw', function () { tooltips(el); });
		$(el).on('click', 'tbody tr', function (e) {
			if (e.target.closest('a')) return;
			var row = table.row(this);
			if (!row.data()) return;
			if (row.child.isShown()) { row.child.hide(); this.classList.remove('shown'); return; }
			row.child(details(row.data())).show();
			this.classList.add('shown');
			tooltips(row.child()[0]);
		});
		return table;
	}

	// Column headers carry the caveat of what they show, once the first page brings the caveats
	function headerTips(el) {
		var keys = { os: 'osVersion', video: 'videoCard', cpus: 'numberOfProcessors', memory: 'memoryStats', client: 'clientOs' };
		el.querySelectorAll('thead th[data-caveat]').forEach(function (th) {
			if (th.dataset.tipped) return;
			var key = keys[th.dataset.caveat];
			if (!caveats[key]) return;
			th.dataset.tipped = '1';
			th.innerHTML = tip(key, th.innerHTML);
		});
		tooltips(el.querySelector('thead'));
	}

	function accountCard(card) {
		var accountId = card.dataset.account;
		var table = reportTable('#sysinfoTable', function () { return { account: accountId }; }, false);
		table.on('xhr', function (e, settings, json) {
			if (json) document.getElementById('sysinfoCount').textContent = json.recordsTotal;
		});
		table.on('draw', function () { headerTips(card); });
	}

	function spreadPage() {
		api.get('/api/client_sysinfo/spread').then(function (d) {
			if (!d.success) return;
			caveats = d.caveats || {};
			trust = d.trust || {};
			var s = d.spread;
			document.getElementById('sysinfoAccounts').textContent = s.accounts;
			[['os', 'osVersion'], ['video', 'videoCard'], ['memory', 'memoryStats'], ['processors', 'numberOfProcessors'], ['clientOs', 'clientOs']].forEach(function (pair) {
				var el = document.getElementById('spread-' + pair[0]);
				if (!el) return;
				var rows = s[pair[0]];
				var html = rows.map(function (e) {
					var pct = s.accounts ? Math.round(e.count * 1000 / s.accounts) / 10 : 0;
					// A video card opens the reports from that card
					var label = pair[0] === 'video' && e.label !== 'Other' && e.label !== '(empty)'
						? '<a href="#sysinfoAll" data-search="' + esc(e.label) + '">' + esc(e.label) + '</a>' : esc(e.label);
					return '<tr><td>' + label + '</td><td class="text-end text-nowrap">' + esc(e.count) + '</td>' +
						'<td style="width:40%"><div class="progress" role="progressbar" aria-label="' + esc(e.label) + '" aria-valuenow="' + pct + '" aria-valuemin="0" aria-valuemax="100">' +
						'<div class="progress-bar" style="width:' + pct + '%"></div></div></td><td class="text-end text-nowrap small">' + pct + '%</td></tr>';
				}).join('') || '<tr><td class="text-body-secondary">Nothing reported yet.</td></tr>';
				el.querySelector('tbody').innerHTML = html;
				var note = el.querySelector('[data-caveat]');
				if (note) note.innerHTML = (trust[pair[1]] === 'unreliable' ? '<span class="text-warning" aria-hidden="true">&#9888;</span> <strong>Unreliable.</strong> ' : '') + esc(caveats[pair[1]] || '');
			});
		}).catch(function () {});

		var latest = document.getElementById('sysinfoLatest');
		var table = reportTable('#sysinfoTable', function () { return { latest: !!(latest && latest.checked) }; }, true);
		table.on('draw', function () { headerTips(document.getElementById('sysinfoAll')); });
		if (latest) latest.addEventListener('change', function () { table.ajax.reload(); });
		document.getElementById('sysinfoPage').addEventListener('click', function (e) {
			var link = e.target.closest('[data-search]');
			if (!link) return;
			e.preventDefault();
			table.search(link.dataset.search).draw();
			document.getElementById('sysinfoAll').scrollIntoView({ behavior: 'smooth' });
		});
	}

	var card = document.getElementById('sysinfoCard');
	if (card) accountCard(card);
	if (document.getElementById('sysinfoPage')) spreadPage();
})();
