/**
 * The "Related" card on account and character pages: everything tied to it (properties, pets, friends, trades and mail,
 * name requests, bug reports, economy flags, cheat detections, dashboard actions), one tab each. The server only sends what the
 * viewer may see. Usage: <div id="relatedCard" data-kind="account|character" data-id="..."></div>
 */
(function () {
	'use strict';
	var card = document.getElementById('relatedCard');
	if (!card) return;
	var kind = card.dataset.kind, id = card.dataset.id;
	// Names come from the server's enums (Labels); only the colours are kept here
	var METHOD_COLOURS = { 1: 'info', 2: 'secondary', 3: 'success', 4: 'light' };
	function methodBadge(value) { return fmt.badge(Labels.name('transferMethods', value) || '?', METHOD_COLOURS[value] || 'secondary'); }
	var FLAG_KIND_COLOURS = { 1: 'warning', 2: 'info', 3: 'danger', 4: 'warning', 5: 'danger' };
	var FLAG_STATUS_COLOURS = { 0: 'danger', 1: 'secondary', 2: 'success' };
	function flagKindBadge(value) { return fmt.badge(Labels.name('flagKinds', value) || '?', FLAG_KIND_COLOURS[value] || 'secondary'); }
	function flagStatusBadge(value) { return fmt.badge(Labels.name('flagStatus', value) || '?', FLAG_STATUS_COLOURS[value] || 'secondary'); }
	var NAME_STATUS = { waiting: ['Waiting for review', 'warning'], approved: ['Approved', 'success'], rejected: ['Rejected', 'danger'], rename_needed: ['New name needed', 'danger'], none: ['', ''] };
	var nf = new Intl.NumberFormat();

	function status(s, reason) {
		var st = NAME_STATUS[s] || [s, 'secondary'];
		if (!st[0]) return '';
		return fmt.badge(st[0], st[1]) + (reason ? '<div class="small text-body-secondary">' + esc(reason) + '</div>' : '');
	}

	function table(headers, rows, empty) {
		if (!rows.length) return '<p class="text-body-secondary mb-0">' + esc(empty) + '</p>';
		return '<div class="table-responsive"><table class="table table-sm table-hover align-middle mb-0 table-stack"><thead><tr>' +
			headers.map(function (h) { return '<th>' + esc(h) + '</th>'; }).join('') + '</tr></thead><tbody>' +
			rows.map(function (r) { return '<tr>' + r.map(function (c, i) { return '<td data-label="' + esc(headers[i]) + '">' + c + '</td>'; }).join('') + '</tr>'; }).join('') +
			'</tbody></table></div>';
	}

	var SECTIONS = [
		['properties', 'Properties', function (rows) {
			return table(kind === 'account' ? ['Property', 'Owner', 'Zone', 'Models', 'Privacy', 'Status', 'Updated'] : ['Property', 'Zone', 'Models', 'Privacy', 'Status', 'Updated'], rows.map(function (p) {
				var cells = [fmt.link('/properties/' + p.id, p.name || '(unnamed)')];
				if (kind === 'account') cells.push(fmt.character(p.owner_id, p.owner_name));
				return cells.concat([fmt.zone(p.zone_id, p.zone_name), esc(nf.format(p.models)), esc(Labels.name('privacy', p.privacy_option) || p.privacy_option),
					p.mod_approved ? fmt.badge('Approved', 'success') : fmt.badge(p.rejection_reason ? 'Rejected' : 'Not approved yet', p.rejection_reason ? 'danger' : 'warning') +
						(p.rejection_reason && !p.mod_approved ? '<div class="small text-body-secondary">' + esc(p.rejection_reason) + '</div>' : ''), esc(fmt.unix(p.last_updated))]);
			}), 'No properties claimed.');
		}],
		['pets', 'Pets', function (rows) {
			return table(kind === 'account' ? ['Pet', 'Owner', 'Name', 'Name status'] : ['Pet', 'Name', 'Name status'], rows.map(function (p) {
				var cells = [esc(p.kind || 'LOT ' + p.lot)];
				if (kind === 'account') cells.push(fmt.character(p.owner_id, p.owner_name));
				return cells.concat([esc(p.name || '(no name)'), status(p.status, p.reason)]);
			}), 'No pets tamed.');
		}],
		['names', 'Name requests', function (rows) {
			return table(kind === 'account' ? ['Character', 'Asked for', 'Status', 'Decided'] : ['Asked for', 'Status', 'Decided'], rows.map(function (r) {
				var cells = kind === 'account' ? [fmt.character(r.character_id, r.character_name)] : [];
				return cells.concat([esc(r.name || '-'), status(r.status, r.reason), r.time ? esc(fmt.unix(r.time)) : '']);
			}), 'No name requests.');
		}],
		['friends', 'Friends', function (rows) {
			return table(['Friend', ''], rows.map(function (f) { return [fmt.character(f.id, f.name), f.best ? fmt.badge('Best friend', 'info') : '']; }), 'No friends.');
		}],
		['transfers', 'Trades & mail', function (rows) {
			// Each item links to its full trace (every hop, where it is now) for staff who can see reports
			var canTrace = window.DASH && DASH.can('reports_view');
			return table(['When', 'How', 'What', 'From', 'To'], rows.map(function (t) {
				var what = t.lot ? esc(nf.format(t.count)) + 'x ' + esc(t.name || 'LOT ' + t.lot) : '';
				if (what && canTrace && t.item_id && t.item_id !== '0') what = '<a href="/reports?object=' + esc(t.item_id) + '" title="Trace this item">' + what + '</a>';
				if (t.coins) what += (what ? ' + ' : '') + esc(nf.format(t.coins)) + ' coins';
				return [esc(fmt.unix(t.time)), methodBadge(t.method), what, fmt.character(t.from_character, t.from_name), fmt.character(t.to_character, t.to_name)];
			}), 'No trades or mail recorded (the last 50 are shown).');
		}],
		['bug_reports', 'Bug reports', function (rows) {
			return table(kind === 'account' ? ['Report', 'By', 'Sent', 'Status'] : ['Report', 'Sent', 'Status'], rows.map(function (r) {
				var cells = [fmt.link('/bug_reports/' + r.id, '#' + r.id + ' ' + r.body)];
				if (kind === 'account') cells.push(fmt.character(r.character_id, r.character_name));
				return cells.concat([esc(r.submitted), r.resolved ? fmt.badge('Resolved', 'success') : fmt.badge('Open', 'warning')]);
			}), 'No bug reports.');
		}],
		['flags', 'Economy flags', function (rows) {
			return table(kind === 'account' ? ['When', 'Kind', 'Character', 'Details', 'Status'] : ['When', 'Kind', 'Details', 'Status'], rows.map(function (f) {
				var cells = [esc(fmt.unix(f.created_at)), flagKindBadge(f.kind)];
				if (kind === 'account') cells.push(fmt.character(f.character_id, f.character_name));
				return cells.concat([esc(f.details), flagStatusBadge(f.status)]);
			}), 'No economy flags.');
		}],
		['chat', 'Chat', function (rows) {
			var CH = { zone: 'Zone', whisper: 'Whisper', team: 'Team', web: 'Web' };
			return table(['When', 'Channel', 'From', 'To', 'Message'], rows.map(function (m) {
				return [esc(fmt.unix(m.time)), fmt.badge(CH[m.channel] || m.channel, 'secondary'), m.sender_id !== '0' ? fmt.character(m.sender_id, m.sender_name) : esc(m.sender_name),
					m.recipient_id !== '0' ? fmt.character(m.recipient_id, m.recipient_name) : '',
					(m.blocked ? fmt.badge('Stopped by the filter', 'danger') + ' ' : '') + esc(m.message)];
			}), 'No chat logged (the newest 100 are shown).');
		}],
		['cheats', 'Cheat detections', function (rows) {
			return table(['When', 'Character', 'What'], rows.map(function (c) { return [esc(c.time), esc(c.name), esc(c.message)]; }), 'No cheat detections.');
		}],
		['audit', 'Staff actions', function (rows) {
			return table(['When', 'By', 'Action', 'Details'], rows.map(function (a) {
				return [esc(fmt.unix(a.time)), esc(a.actor), '<code>' + esc(a.action) + '</code>', esc(a.description)];
			}), 'No dashboard actions recorded about this account.');
		}]
	];

	function render(related) {
		var tabs = SECTIONS.filter(function (s) { return related[s[0]] !== undefined; });
		var logs = related.logs || {};
		var links = [];
		if (logs.activity) links.push(fmt.link('/activity_log#search=' + id, 'Activity log'));
		if (logs.commands) links.push(fmt.link('/command_log#search=' + id, 'Command log'));
		card.innerHTML = '<div class="card-header d-flex flex-wrap justify-content-between align-items-center gap-2"><h5 class="mb-0">Related</h5>' +
			(links.length ? '<span class="small">' + links.join(' &middot; ') + '</span>' : '') + '</div>' +
			'<div class="card-body"><ul class="nav nav-tabs mb-3" role="tablist">' + tabs.map(function (s, i) {
				return '<li class="nav-item" role="presentation"><button class="nav-link' + (i === 0 ? ' active' : '') + '" data-bs-toggle="tab" data-bs-target="#related-' + s[0] + '" type="button" role="tab">' +
					esc(s[1]) + ' <span class="badge text-bg-secondary">' + related[s[0]].length + '</span></button></li>';
			}).join('') + '</ul><div class="tab-content">' + tabs.map(function (s, i) {
				return '<div class="tab-pane' + (i === 0 ? ' show active' : '') + '" id="related-' + s[0] + '" role="tabpanel">' + s[2](related[s[0]]) + '</div>';
			}).join('') + '</div></div>';
	}

	api.get('/api/' + (kind === 'account' ? 'accounts' : 'characters') + '/' + id + '/related').then(function (d) {
		if (d.success) render(d.related);
		else card.innerHTML = '<div class="card-body text-body-secondary">' + esc(d.error || 'Could not load related data') + '</div>';
	});
})();
