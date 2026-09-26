/**
 * Plays property model behaviors in the 3D viewer, following the server's rules (dGame/dPropertyBehaviors/Strip.cpp):
 * - every strip starts with a trigger block (On Interact, On Attack, On Chat) and waits for it;
 * - then runs its actions in order and goes back to waiting; all strips of the current state run side by side;
 * - moves are along the world axes at the model's speed (until Set Speed);
 * - Change State switches every strip of that behavior to the new state's strips; Restart resets the model.
 * Which blocks exist and what they do (triggers, moves, spawns, drops, state changes, the default speed) comes from
 * the server's own table (PropertyBehaviorActions.h), passed to createBehaviorPlayer as `rules` (BehaviorXml::Rules).
 * Things the viewer can't show (sounds, spawned enemies, drops) are written to the log instead.
 */
import * as THREE from 'three';

// {triggers, moves: {type: [axis, sign]}, spawns: {type: {lot, name}}, drops, stateChanges: {type: stateId}, others, states, defaultSpeed}
let rules = { triggers: [], moves: {}, spawns: {}, drops: {}, stateChanges: {}, others: [], states: {}, defaultSpeed: 0 };

// How an action reads in the list: "Move right 5", "Wait 1s", "On chat: hello"
export function describeAction(action) {
	let value = action.value !== undefined && action.value !== '' ? action.value : null;
	// Message is the only text parameter (Action.cpp); numbers are stored as doubles ("0.20000000000000001"): show them the way a player typed them
	const text = action.parameter === 'Message';
	if (value !== null && !text && isFinite(Number(value))) value = String(Number(Number(value).toFixed(3)));
	const words = action.type.replace(/^On/, 'On ').replace(/([a-z])([A-Z])/g, '$1 $2').toLowerCase();
	const label = words.charAt(0).toUpperCase() + words.slice(1);
	if (action.type === 'Wait') return 'Wait ' + (value || 0) + 's';
	if (action.type === 'UnSmash') return 'Unsmash' + (value ? ' over ' + value + 's' : '');
	if (text) return label + (value !== null ? ': "' + value + '"' : '');
	if (action.type === 'PlaySound') return 'Play sound ' + (value || '');
	return label + (value !== null ? ' ' + value : '');
}

export function isTrigger(action) {
	return !!action && rules.triggers.includes(action.type);
}

export function isSupported(action) {
	return isTrigger(action) || action.type in rules.stateChanges || action.type in rules.moves || action.type in rules.spawns ||
		action.type in rules.drops || rules.others.includes(action.type);
}

export function createBehaviorPlayer(viewer, serverRules, { onLog, onChange } = {}) {
	rules = serverRules;
	let running = false;
	let runtimes = []; // per model index: {speed, offset, visible, behaviors: [{def, state, strips: [{actions, index, waiting, pause, move}]}]}

	const log = (modelIndex, text) => { if (onLog) onLog(modelIndex, text); };
	const changed = () => { if (onChange) onChange(); };

	function stripsFor(def, stateId) {
		const state = def.states.find((s) => s.id === stateId) || { strips: [] };
		// A strip needs a trigger and at least one action to run (Strip::HasMinimumActions)
		return state.strips.filter((strip) => strip.actions.length >= 2).map((strip) => ({ actions: strip.actions, index: 0, waiting: true, pause: 0, move: null }));
	}

	function reset() {
		const models = viewer.models();
		runtimes = models.map((model, i) => {
			viewer.setModelState(i, new THREE.Vector3(), true);
			return {
				speed: rules.defaultSpeed, offset: new THREE.Vector3(), visible: true,
				behaviors: (model.behaviors || []).map((def) => ({ def, state: 0, strips: stripsFor(def, 0) }))
			};
		});
		changed();
	}

	/**
	 * Fire a trigger on one model (OnInteract, OnAttack, or OnChat with the message). Like the server (Strip::OnChatMessageReceived
	 * and friends): a strip reacts when its next action is that trigger, it's waiting for it, and it isn't paused; only the
	 * current state's strips are there to react. Returns how many strips started.
	 */
	function trigger(modelIndex, type, message) {
		const runtime = runtimes[modelIndex];
		if (!runtime) return 0;
		let started = 0;
		for (const behavior of runtime.behaviors) {
			for (const strip of behavior.strips) {
				const next = strip.actions[strip.index];
				if (!strip.waiting || strip.pause > 0 || !next || next.type !== type) continue;
				if (type === 'OnChat' && String(next.value || '') !== String(message || '')) continue;
				strip.waiting = false;
				strip.index++;
				started++;
			}
		}
		if (started) {
			running = true;
			log(modelIndex, describeAction({ type, value: type === 'OnChat' ? message : undefined }) + ' → ' + started + ' strip' + (started === 1 ? '' : 's'));
		}
		changed();
		return started;
	}

	/**
	 * Say something in chat: like in game, every model hears it, so every strip waiting for that phrase starts at once.
	 * Returns {models, strips}: how many models reacted and how many strips started.
	 */
	function chatAll(message) {
		let models = 0, strips = 0;
		for (let i = 0; i < runtimes.length; i++) {
			const started = trigger(i, 'OnChat', message);
			if (started) { models++; strips += started; }
		}
		return { models, strips };
	}

	function run(modelIndex, runtime, behavior, strip) {
		const action = strip.actions[strip.index];
		const number = Number(action.value) || 0;
		let stateChange = null, heard = null;
		// A trigger in the middle of a strip: wait there for it
		if (isTrigger(action)) {
			strip.waiting = true;
			return null;
		}
		if (action.type in rules.moves) {
			const [axis, sign] = rules.moves[action.type];
			strip.move = { axis, remaining: Math.abs(number), sign };
		} else if (action.type === 'SetSpeed') {
			runtime.speed = number || rules.defaultSpeed;
		} else if (action.type === 'Wait') {
			strip.pause = number;
		} else if (action.type === 'Smash') {
			runtime.visible = false;
			log(modelIndex, 'Smashed');
		} else if (action.type === 'UnSmash') {
			runtime.visible = true;
			strip.pause = number + 0.5;
			log(modelIndex, 'Rebuilt');
		} else if (action.type === 'Chat') {
			// Every model on the property hears what a model says, itself too (PropertyManagementComponent::OnChatMessageReceived)
			viewer.say(modelIndex, action.value || '');
			log(modelIndex, 'Says "' + (action.value || '') + '"');
			heard = action.value || '';
		} else if (action.type === 'PrivateMessage') {
			// Not shown in chat, but the models hear it all the same
			log(modelIndex, 'Sends a private message "' + (action.value || '') + '"');
			heard = action.value || '';
		} else if (action.type === 'PlaySound') {
			log(modelIndex, 'Plays sound ' + number);
		} else if (action.type in rules.spawns) {
			viewer.say(modelIndex, 'Spawns ' + rules.spawns[action.type].name, 2);
			log(modelIndex, 'Spawns ' + rules.spawns[action.type].name);
		} else if (action.type in rules.drops) {
			// Every player gets one powerup per unit
			log(modelIndex, 'Drops ' + number + ' × ' + rules.drops[action.type].name);
		} else if (action.type in rules.stateChanges) {
			stateChange = rules.stateChanges[action.type];
		} else if (action.type === 'Restart') {
			log(modelIndex, 'Restarts');
			return 'restart';
		} else {
			log(modelIndex, describeAction(action) + ' is not supported by the server; skipped');
		}
		strip.index++;
		if (strip.index >= strip.actions.length) { strip.index = 0; strip.waiting = true; }
		if (heard !== null) {
			const reacted = chatAll(heard);
			if (reacted.models) log(modelIndex, '"' + heard + '" started ' + reacted.strips + ' strip' + (reacted.strips === 1 ? '' : 's') + ' on ' + reacted.models + ' model' + (reacted.models === 1 ? '' : 's'));
		}
		if (stateChange !== null) {
			behavior.state = stateChange;
			behavior.strips = stripsFor(behavior.def, stateChange);
			log(modelIndex, 'Changes to the ' + (rules.states[stateChange] || 'state ' + stateChange) + ' state');
		}
		return null;
	}

	function tick(dt) {
		if (!running) return;
		let active = false, dirty = false;
		runtimes.forEach((runtime, modelIndex) => {
			let moved = false, restart = false;
			for (const behavior of runtime.behaviors) {
				for (const strip of [...behavior.strips]) {
					if (strip.waiting) continue;
					active = true;
					if (strip.move) {
						const step = Math.min(strip.move.remaining, runtime.speed * dt);
						runtime.offset[strip.move.axis] += step * strip.move.sign;
						strip.move.remaining -= step;
						moved = true;
						if (strip.move.remaining > 1e-6) continue;
						strip.move = null;
					}
					if (strip.pause > 0) { strip.pause -= dt; continue; }
					const before = runtime.visible;
					if (run(modelIndex, runtime, behavior, strip) === 'restart') restart = true;
					if (runtime.visible !== before) moved = true;
					dirty = true;
					if (!behavior.strips.includes(strip)) break; // the state changed
				}
			}
			if (restart) {
				runtime.speed = rules.defaultSpeed;
				runtime.offset.set(0, 0, 0);
				runtime.visible = true;
				runtime.behaviors.forEach((b) => { b.state = 0; b.strips = stripsFor(b.def, 0); });
				moved = true;
			}
			if (moved) viewer.setModelState(modelIndex, runtime.offset, runtime.visible);
		});
		if (dirty) changed();
		if (!active) running = false;
	}

	return {
		reset,
		trigger,
		chatAll,
		tick,
		isRunning: () => running,
		/** What each strip is doing, for the panel: [{behavior, state, strips: [{index, waiting}]}] */
		status(modelIndex) {
			const runtime = runtimes[modelIndex];
			return runtime ? runtime.behaviors.map((b) => ({ state: b.state, strips: b.strips.map((s) => ({ index: s.index, waiting: s.waiting, actions: s.actions })) })) : [];
		}
	};
}
