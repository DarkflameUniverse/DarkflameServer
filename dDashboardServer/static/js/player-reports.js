/**
 * The Player Reports page: reports players sent from the game, newest first, filtered by status (and by the account
 * they're about with ?account=). Staff who may handle them act on one (with the usual reject dialog, which can add a
 * strike) or dismiss it. Kinds and statuses are named by the server.
 */
(function () {
	'use strict';

	var PAGE = 50;
	var params = new URLSearchParams(location.search);
	var state = { status: params.has('status') ? params.get('status') : '0', account: params.get('account') || '', offset: 0, statuses: null };
	var STATUS_STYLE = { OPEN: 'warning', ACTIONED: 'success', DISMISSED: 'secondary' };
	var reports = {};

	function renderStatuses(statuses) {
		if (state.statuses) return;
		state.statuses = statuses;
		var options = statuses.concat([{ value: '', name: 'All' }]);
		document.getElementById('statusFilter').innerHTML = options.map(function (s) {
			var id = 'status' + (s.value === '' ? 'All' : s.value);
			return '<input type="radio" class="btn-check" name="status" id="' + id + '" value="' + esc(s.value) + '"' + (String(s.value) === state.status ? ' checked' : '') + '>' +
				'<label class="btn btn-outline-primary" for="' + id + '">' + esc(s.name) + '</label>';
		}).join('');
	}

	function about(r) {
		var parts = [];
		if (r.target_character_id !== '0') parts.push(fmt.character(r.target_character_id, r.target_name));
		if (r.target_account_id) parts.push('<a class="small" href="/accounts/' + esc(r.target_account_id) + '">' + esc(r.target_account_name || ('Account ' + r.target_account_id)) + '</a>');
		if (r.property_id !== '0') parts.push('<span class="small">' + fmt.property(r.property_id, r.property_name || 'Property') + '</span>');
		if (r.kind !== 'PLAYER' && r.object_lot) parts.push('<span class="small text-body-secondary">' + esc(r.object_name || ('LOT ' + r.object_lot)) + ' (' + esc(r.object_id) + ')</span>');
		return parts.join('<br>') || '<span class="text-body-secondary">Unknown</span>';
	}

	function row(r, canManage) {
		reports[r.id] = r;
		var status = fmt.badge(r.status_name, STATUS_STYLE[r.status_key] || 'secondary');
		var handled = r.handled_at ? '<div class="small text-body-secondary">' + esc(r.handled_by) + ', ' + esc(fmt.unix(r.handled_at)) + '</div>' +
			(r.resolution ? '<div class="small">' + esc(r.resolution) + '</div>' : '') : '';
		var actions = '';
		if (canManage && r.status_key === 'OPEN') {
			actions = '<div class="d-flex gap-1 flex-wrap"><button type="button" class="btn btn-sm btn-outline-danger text-nowrap" data-act="' + esc(r.id) + '">Act on it</button>' +
				'<button type="button" class="btn btn-sm btn-outline-secondary" data-dismiss-report="' + esc(r.id) + '">Dismiss</button>' + AiSuggest.button('player_report', r.id) + '</div>';
		}
		return '<tr><td class="text-nowrap small">' + esc(fmt.unix(r.created_at)) + '</td>' +
			'<td>' + fmt.badge(r.kind_name, 'secondary') + '</td>' +
			'<td>' + fmt.character(r.reporter_id, r.reporter_name) + '</td>' +
			'<td>' + about(r) + '</td>' +
			'<td class="small">' + (r.zone_id ? fmt.zone(r.zone_id, r.zone_name) : '') + '</td>' +
			'<td style="white-space: pre-wrap; max-width: 28rem">' + esc(r.body) + '</td>' +
			'<td>' + status + handled + '</td><td>' + actions + '</td></tr>';
	}

	function load() {
		var query = new URLSearchParams({ limit: PAGE, offset: state.offset });
		if (state.status !== '') query.set('status', state.status);
		if (state.account) query.set('account', state.account);
		api.get('/api/player_reports?' + query).then(function (d) {
			if (!d.success) return;
			renderStatuses(d.statuses);
			reports = {};
			document.getElementById('reportRows').innerHTML = d.reports.map(function (r) { return row(r, d.canManage); }).join('') ||
				'<tr><td colspan="8" class="text-body-secondary">No reports.</td></tr>';
			document.getElementById('reportCount').textContent = d.total ? (state.offset + 1) + '–' + (state.offset + d.reports.length) + ' of ' + d.total : '';
			document.getElementById('prevPage').disabled = state.offset === 0;
			document.getElementById('nextPage').disabled = state.offset + d.reports.length >= d.total;
			var filter = document.getElementById('accountFilter');
			filter.classList.toggle('d-none', !state.account);
			if (state.account) filter.innerHTML = 'Only reports about <a href="/accounts/' + esc(state.account) + '">account ' + esc(state.account) + '</a>. <a href="/player_reports">Show all</a>';
		}).catch(function () {});
	}

	document.getElementById('statusFilter').addEventListener('change', function (e) {
		state.status = e.target.value;
		state.offset = 0;
		load();
	});
	document.getElementById('prevPage').addEventListener('click', function () { state.offset = Math.max(0, state.offset - PAGE); load(); });
	document.getElementById('nextPage').addEventListener('click', function () { state.offset += PAGE; load(); });

	document.getElementById('reportRows').addEventListener('click', function (e) {
		var act = e.target.closest('[data-act]'), dismiss = e.target.closest('[data-dismiss-report]');
		if (act) {
			var r = reports[act.dataset.act];
			Decide({ title: 'Act on report #' + r.id, action: 'Mark acted on', reasonLabel: 'What was done (kept with the report)', characterId: r.target_character_id,
				text: r.target_account_id ? 'Mute, warn or ban the player from their account page; this closes the report.' : 'The game didn\'t say whose this is.' }).then(function (choice) {
				if (!choice) return;
				api.action('/api/player_reports/' + r.id + '/action', { reason: choice.reason, strike: choice.strike }).then(function (d) { toast(d.message, 'success'); }).catch(function () {});
			});
		} else if (dismiss) {
			var id = dismiss.dataset.dismissReport;
			Decide({ title: 'Dismiss report #' + id, action: 'Dismiss', reasonLabel: 'Why (optional)' }).then(function (choice) {
				if (!choice) return;
				api.action('/api/player_reports/' + id + '/dismiss', { reason: choice.reason }).then(function (d) { toast(d.message, 'success'); }).catch(function () {});
			});
		}
	});

	if (window.Live) Live.on('player_reports', load);
	load();
})();
