/*
 * \brief  Genodians manager
 * \author Josef Soentgen
 * \date   2025-01-02
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <os/buffered_xml.h>
#include <os/path.h>
#include <os/reporter.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>

/* sculpt_manager includes */
#include <model/child_exit_state.h>
#include <model/child_state.h>

/* musl includes */
#include <tm.h>


namespace Utils {
	using namespace Genode;

	struct Seconds {

		uint64_t value;

		void print(Output &out) const
		{
			unsigned const d  = unsigned(value / 86400);
			unsigned const dr = unsigned(value % 86400);
			unsigned const h  = unsigned(dr / 3600);
			unsigned const m  = unsigned(dr / 60 % 60);
			unsigned const s  = unsigned(dr % 60);

			if (d) Genode::print(out, d, " day",    d > 1 ? "s " : " ");
			if (h) Genode::print(out, h, " hour",   h > 1 ? "s " : " ");
			if (m) Genode::print(out, m, " minute", m > 1 ? "s " : " ");
			if (s) Genode::print(out, s, " second", s > 1 ? "s"  : "");

			if (!(d||h||m||s)) Genode::print(out, "0 seconds");
		}

		static Seconds from_rtc(Rtc::Timestamp const &);

		Seconds diff(Seconds const &other) const
		{
			return { other.value - value };
		}
	};

	Rtc::Timestamp from_seconds(Seconds const&);

	using Date = String<21>;
	Date from_rtc(Rtc::Timestamp const &ts)
	{
		bool const iso_8601 = true;

		bool const pad_month  = ts.month  < 10;
		bool const pad_day    = ts.day    < 10;
		bool const pad_hour   = ts.hour   < 10;
		bool const pad_minute = ts.minute < 10;
		bool const pad_second = ts.second < 10;

		return Date(ts.year, "-",
		            pad_month  ? "0" : "", ts.month,  "-",
		            pad_day    ? "0" : "", ts.day,    iso_8601 ? "T" : " ",
		            pad_hour   ? "0" : "", ts.hour,   ":",
		            pad_minute ? "0" : "", ts.minute, ":",
		            pad_second ? "0" : "", ts.second, iso_8601 ? "Z" : "");
	}

	namespace Html {
		using String = String<64>;
		void gen_section_div(Xml_generator &xml, char const *name, auto const &fn);
		void gen_table_body(Xml_generator &xml, auto const &fn);
		void gen_table_key_value_row(Xml_generator &, Html::String const &, Html::String const &);
	} /* namespace Html */
} /* namespace Utils */


Utils::Seconds Utils::Seconds::from_rtc(Rtc::Timestamp const &rtc)
{
	struct tm tm;
	Genode::memset(&tm, 0, sizeof(struct tm));
	tm.tm_sec  = rtc.second;
	tm.tm_min  = rtc.minute;
	tm.tm_hour = rtc.hour;
	tm.tm_mday = rtc.day;
	tm.tm_mon  = rtc.month - 1;
	tm.tm_year = rtc.year - 1900;

	return Seconds { .value = Genode::uint64_t(tm_to_secs(&tm)) };
}


Rtc::Timestamp Utils::from_seconds(Utils::Seconds const &seconds)
{
	struct tm tm;
	Genode::memset(&tm, 0, sizeof(tm));

	int const err = secs_to_tm(seconds.value, &tm);
	if (err)
		return Rtc::Timestamp();

	return Rtc::Timestamp {
		.microsecond = 0,
		.second = (unsigned)tm.tm_sec,
		.minute = (unsigned)tm.tm_min,
		.hour   = (unsigned)tm.tm_hour,
		.day    = (unsigned)tm.tm_mday,
		.month  = (unsigned)tm.tm_mon  + 1,
		.year   = (unsigned)tm.tm_year + 1900,
	};
}


void Utils::Html::gen_section_div(Xml_generator &xml, char const *name, auto const &fn)
{
	xml.node("div", [&] {
		xml.attribute("id", name);
		xml.node("h3", [&] { xml.append(name); });
		fn(xml); });
}


void Utils::Html::gen_table_body(Xml_generator &xml, auto const &fn) {
	xml.node("table", [&] { xml.node("tbody", [&] { fn(xml); }); }); }


void Utils::Html::gen_table_key_value_row(Xml_generator &xml, Utils::Html::String const &key,
                                                              Utils::Html::String const &value)
{
	xml.node("tr", [&] {
		xml.node("td", [&] { xml.append(key.string()); });
		xml.node("td", [&] { xml.append(value.string()); });
	});
}


/*
 * Extend helper utilities from Sculpt
 */
namespace Sculpt {
	void gen_default_route_parent(Generator &g)
	{
		g.node("default-route", [&] {
			g.node("any-service", [&] {
				g.node("parent", [&] { }); }); });
	}

	void gen_common_parent_routes(Generator &g)
	{
		gen_service_node<Rom_session>(g, [&] { g.node("parent", [&] {}); });
		gen_service_node<Cpu_session>(g, [&] { g.node("parent", [&] {}); });
		gen_service_node<Pd_session> (g, [&] { g.node("parent", [&] {}); });
		gen_service_node<Rm_session> (g, [&] { g.node("parent", [&] {}); });
		gen_service_node<Log_session>(g, [&] { g.node("parent", [&] {}); });
	}

	void gen_arg(Generator &g, auto const &arg) {
		g.node("arg", [&] { g.attribute("value", arg); }); }

	void gen_named_dir(Generator &g, char const *name, auto const &fn)
	{
		g.node("dir", [&] {
			g.attribute("name", name);
			fn(g); });
	}

	void gen_symlink(Generator &g, char const *name, char const *target)
	{
		g.node("symlink", [&] {
			g.attribute("name", name);
			g.attribute("target", target); });
	}

	void gen_rom(Generator &g, char const *name)
	{
		g.node("rom", [&] {
			g.attribute("name", name);
			g.attribute("binary", "no"); });
	}

	void gen_heartbeat_node(Generator &g, unsigned rate_ms) {
		g.node("heartbeat", [&] { g.attribute("rate_ms", rate_ms); }); }
} /* namespace Sculpt */


namespace Genodians {
	using namespace Genode;
	using namespace Sculpt;
	using namespace Utils;

	struct Notify_interface : Interface
	{
		virtual void notify() const = 0;
	};

	/*
	 * The Managed_init interface provides mechanisms for
	 * monitoring and updating the managed init.
	 */
	struct Managed_init : Interface
	{
		Allocator &_alloc;

		Notify_interface &_state_change_notifier;

		using Child_state_registery = Registry<Child_state>;
		Child_state_registery child_states { };

		bool _evalute_child_states(Node const &state)
		{
			/* upgrade RAM and cap quota on demand */
			bool reconfigure = false;
			state.for_each_sub_node("child", [&] (Node const &child) {
				bool reconfiguration_needed = false;
				child_states.for_each([&] (Child_state &child_state) {
					if (child_state.apply_child_state_report(child))
						reconfiguration_needed = true; });
				if (reconfiguration_needed)
					reconfigure = true;
			});
			return reconfigure;
		}

		Constructible<Buffered_node> _cached_state_report { };

		bool _update_cached_state_report(Node const &node)
		{
			_cached_state_report.construct(_alloc, node);
			return true;
		}

		Rom_handler<Managed_init> _state_rom;

		void _state_update(Node const &node) {

			if (Managed_init::_update_cached_state_report(node))
				_state_change_notifier.notify();

			state_update(node, _evalute_child_states(node)); }

		Expanding_reporter _config_reporter;

		Managed_init(Env &env, Allocator &alloc, Notify_interface &notify,
		             char const *state_name, char const *config_name)
		:
			_alloc                 { alloc },
			_state_change_notifier { notify },
			_state_rom             { env, state_name, *this,
			                         &Managed_init::_state_update },
			_config_reporter       { env, "config", config_name }
		{ }

		void generate_config(auto const &fn)
		{
			_config_reporter.generate([&] (Generator &g) {
				fn(g); });
		}

		void with_cached_state_report(auto const &avail_fn,
		                              auto const &missing_fn) const
		{
			if (_cached_state_report.constructed())
				avail_fn(*_cached_state_report);
			else
				missing_fn();
		}

		/****************************
		 ** Managed_init interface **
		 ****************************/

		virtual void generate_report(Xml_generator &)    const = 0;
		virtual void state_update   (Node const &, bool)       = 0;
	};

	struct Import;
	struct Lighttpd;

	/*
	 * The Managed_child interface streamlines the generation
	 * of the configuration for each step of the import process
	 * and provides a convenience state checker.
	 *
	 */
	struct Managed_child : Interface
	{
		struct Ok    { bool finished;   };
		struct Error { int  exit_value; };

		using Result = Attempt<Ok, Error>;

		Child_state _child_state;

		Managed_child(Managed_init::Child_state_registery &registry,
		              char const *name, Priority priority,
		              Ram_quota ram, Cap_quota caps)
		:
			_child_state { registry, name, priority, ram, caps }
		{ }

		virtual ~Managed_child() { }

		Result check(Node const &state_node)
		{
			Child_exit_state const exit_state(state_node, name());

			if (!exit_state.responsive)
				return Error { .exit_value = -42 };
			if (!exit_state.exited)
				return Ok { .finished = false };
			if (exit_state.code != 0) {
				error(name(), " exited with value ", exit_state.code);
				return Error { .exit_value = exit_state.code };
			}
			return Ok { .finished = true };
		}

		void gen_start_node_content(Generator &g) const {
			_child_state.gen_start_node_content(g); }

		void trigger_restart() {
			_child_state.trigger_restart(); }

		Start_name const name() const {
			return _child_state.name(); }

		/*****************************
		 ** Managed_child interface **
		 *****************************/

		virtual void  generate(Generator &g) const = 0;
	};

	struct Fetch;
	struct Wipe;
	struct Extract;
	struct Generate;

	struct Config
	{
		struct Child
		{
			Ram_quota ram;
			Cap_quota caps;

			static Child from_node(Node const &node)
			{
				return Child {
					.ram = Ram_quota {
						node.attribute_value("ram", Number_of_bytes(48u << 20)) },
					.caps = Cap_quota {
						node.attribute_value("caps", 300u) }
				};
			}
		};

		struct Lighttpd
		{
			Child    lighttpd;
			unsigned heartbeat_ms;
		};

		struct Import
		{
			Child    fetchurl;
			Child    wipe;
			Child    extract;
			Child    generate;
			unsigned sleep_duration;
			unsigned heartbeat_ms;
		};

		unsigned status_update_interval;

		Lighttpd lighttpd_config;
		Import   import_config;

		static Config update_from_node(Node const &config_node)
		{
			unsigned const status_update_interval =
				config_node.attribute_value("status_update_interval_sec", 60u);

			Child const lighttpd =
				config_node.with_sub_node("lighttpd",
					[&] (Node const &node) { return Child::from_node(node); },
					[&]                    { return Child::from_node(Node()); });
			unsigned const lighttpd_heartbeat_ms =
				config_node.with_sub_node("lighttpd",
					[&] (Node const &node) { return node.attribute_value("heartbeat_ms", 3000u); },
					[&]                    { return 3000u; });

			Import const import_config =
				config_node.with_sub_node("import",
					[&] (Node const &node) {

						unsigned const import_heartbeat_ms =
							node.attribute_value("heartbeat_ms", 3000u);
						unsigned const sleep_duration =
							node.attribute_value("update_interval_min", 180u);

						Child const fetchurl =
							node.with_sub_node("fetchurl",
								[&] (Node const &node) { return Child::from_node(node); },
								[&]                    { return Child::from_node(Node()); });
						Child const wipe =
							node.with_sub_node("wipe",
								[&] (Node const &node) { return Child::from_node(node); },
								[&]                    { return Child::from_node(Node()); });
						Child const extract =
							node.with_sub_node("extract",
								[&] (Node const &node) { return Child::from_node(node); },
								[&]                    { return Child::from_node(Node()); });
						Child const generate =
							node.with_sub_node("generate",
								[&] (Node const &node) { return Child::from_node(node); },
								[&]                    { return Child::from_node(Node()); });

						return Import {
							.fetchurl       = fetchurl,
							.wipe           = wipe,
							.extract        = extract,
							.generate       = generate,
							.sleep_duration = sleep_duration,
							.heartbeat_ms   = import_heartbeat_ms
						};
					},
					[&] { return Import { }; });

			return Config {
				.status_update_interval = status_update_interval,
				.lighttpd_config = Lighttpd {
					.lighttpd     = lighttpd,
					.heartbeat_ms = lighttpd_heartbeat_ms
				},
				.import_config   = import_config
			};
		}
	};

	struct Main;
} /* namespace Genodians */


struct Genodians::Fetch : Genodians::Managed_child
{
	Fetch(Managed_init::Child_state_registery &registry,
	      Config::Child                 const &config)
	:
		Managed_child { registry, "fetchurl",
		                Priority  { 0 },
		                config.ram, config.caps }
	{ }

	/*****************************
	 ** Managed_child interface **
	 *****************************/

	void generate(Generator &g) const override
	{
		g.node("start", [&] {
			gen_start_node_content(g);

			g.node("heartbeat", [&] { });

			g.node("route", [&] {
				gen_service_node<Rom_session>(g, [&] {
					g.attribute("label", "config");
					g.node("parent", [&] {
						g.attribute("label", "fetchurl.config"); }); });
				gen_service_node<Nic::Session>(g, [&] {
					g.node("parent", [&] { }); });
				gen_service_node<File_system::Session>(g, [&] {
					g.node("parent", [&] { }); });
				gen_service_node<Timer::Session>(g, [&] {
					g.node("parent", [&] { }); });
				gen_common_parent_routes(g);
			});
		});
	}
};


struct Genodians::Wipe : Genodians::Managed_child
{
	Wipe(Managed_init::Child_state_registery &registry,
	     Config::Child                 const &config)
	:
		Managed_child { registry, "wipe",
		                Priority  { 0 },
		                config.ram, config.caps }
	{ }

	/*****************************
	 ** Managed_child interface **
	 *****************************/

	void generate(Generator &g) const override
	{
		g.node("start", [&] {
			gen_start_node_content(g);
			gen_named_node(g, "binary", "init");

			g.node("heartbeat", [&] { });

			g.node("route", [&] {
				gen_service_node<Rom_session>(g, [&] {
					g.attribute("label", "config");
					g.node("parent", [&] {
						g.attribute("label", "wipe.config"); }); });
				gen_service_node<File_system::Session>(g, [&] {
					g.node("parent", [&] { }); });
				gen_service_node<Timer::Session>(g, [&] {
					g.node("parent", [&] { }); });
				gen_common_parent_routes(g);
			});
		});
	}
};


struct Genodians::Extract : Genodians::Managed_child
{
	Extract(Managed_init::Child_state_registery &registry,
	        Config::Child                 const &config)
	:
		Managed_child { registry, "extract",
		                Priority  { 0 },
		                config.ram, config.caps }
	{ }

	/*****************************
	 ** Managed_child interface **
	 *****************************/

	void generate(Generator &g) const override
	{
		g.node("start", [&] {
			gen_start_node_content(g);

			g.node("heartbeat", [&] { });

			g.node("route", [&] {
				gen_service_node<File_system::Session>(g, [&] {
					g.node("parent", [&] { }); });
				gen_service_node<Rom_session>(g, [&] {
					g.attribute("label", "config");
					g.node("parent", [&] {
						g.attribute("label", "extract.config"); }); });
				gen_common_parent_routes(g);
			});
		});
	}
};


struct Genodians::Generate : Genodians::Managed_child
{
	Generate(Managed_init::Child_state_registery &registry,
	         Config::Child                 const &config)
	:
		Managed_child { registry, "generate",
		                Priority  { 0 },
		                config.ram, config.caps }
	{ }

	/*****************************
	 ** Managed_child interface **
	 *****************************/

	void generate(Generator &g) const override
	{
		g.node("start", [&] {
			gen_start_node_content(g);

			gen_named_node(g, "binary", "init");
			g.node("heartbeat", [&] { });

			g.node("route", [&] {
				gen_service_node<Rom_session>(g, [&] {
					g.attribute("label", "config");
					g.node("parent", [&] {
						g.attribute("label", "generate.config"); }); });
				gen_service_node<File_system::Session>(g, [&] {
					g.node("parent", [&] { }); });
				gen_service_node<Timer::Session>(g, [&] {
					g.node("parent", [&] { }); });
				gen_service_node<Rtc::Session>(g, [&] {
					g.node("parent", [&] { }); });
				gen_common_parent_routes(g);
			});
		});
	}
};


struct Genodians::Import : Genodians::Managed_init
{
	Env &_env;

	Timer::Connection &_timer;
	Rtc::Connection   &_rtc;

	enum class State {
		INVALID, INIT, FETCH, WIPE, EXTRACT, GENERATE, SLEEP };

	State _state;

	Constructible<Fetch>    _fetch    { };
	Constructible<Wipe>     _wipe     { };
	Constructible<Extract>  _extract  { };
	Constructible<Generate> _generate { };

	Seconds _last_fetch_duration    {  60u };
	Seconds _last_wipe_duration     {  15u };
	Seconds _last_extract_duration  {  15u };
	Seconds _last_generate_duration { 180u };

	/*
	 * Step timeout handling
	 */

	Seconds _calculate_timeout(Seconds const secs,
	                           Seconds const min = { .value = 15u }) const
	{
		/* some steps take normally at most a few seconds */
		Seconds const min_duration = { max(secs.value / 2, min.value) };

		return { secs.value + min_duration.value };
	}

	Seconds _step_timeout_secs { 0 };
	bool    _step_timeout_triggered = false;

	Timer::One_shot_timeout<Import> _step_timeout {
		_timer, *this, &Import::_handle_step_timeout };

	void _handle_step_timeout(Duration)
	{
		_step_timeout_triggered = true;
		warning("timeout triggered for step ", (unsigned)_state);

		with_cached_state_report([&] (Node const &node) {
			state_update(node, false); }, [&] { });
	}

	/*
	 * Sleep timeout handling
	 */

	bool _sleep_timeout_triggered = false;

	Timer::One_shot_timeout<Import> _sleep_timeout {
		_timer, *this, &Import::_handle_sleep_timeout };

	void _handle_sleep_timeout(Duration)
	{
		_sleep_timeout_triggered = true;

		with_cached_state_report([&] (Node const &node) {
			state_update(node, false); }, [&] { });
	}

	void _update_init_config(Generator &g);

	Config::Import const &_config;

	Date _last_update { };
	Date _next_update { };

	Seconds  _import_start      { 0 };
	Seconds  _import_step_start { 0 };
	Seconds  _import_duration   { 0 };
	unsigned _imports           = 0;

	Import(Env                  &env,
	       Allocator            &alloc,
	       Notify_interface     &notify,
	       Timer::Connection    &timer,
	       Rtc::Connection      &rtc,
	       Config::Import const &config)
	:
		Managed_init { env, alloc, notify,
		               "import.state", "import.config" },
		_env         { env },
		_timer       { timer },
		_rtc         { rtc },
		_state       { State::INIT },
		_config      { config }
	{
		/* initial Rom_handler signal will get us started */
	}

	/****************************
	 ** Managed_init interface **
	 ****************************/

	void generate_report(Xml_generator &xml)     const override;
	void state_update   (Node const &, bool)     override;
};


void Genodians::Import::generate_report(Xml_generator &xml) const
{
	Html::gen_section_div(xml, "Import", [&] (Xml_generator &xml) {
		if (_imports) {
			Html::gen_table_body(xml, [&] (Xml_generator &xml) {
				Html::gen_table_key_value_row(xml, Html::String("Total imports"),
				                                   _imports);
				Html::gen_table_key_value_row(xml, Html::String("Import duration"),
				                                   _import_duration);
				Html::gen_table_key_value_row(xml, Html::String("Last import"),
				                                   _last_update);
				Html::gen_table_key_value_row(xml, Html::String("Next import"),
				                                   _next_update);
			});
			xml.node("p", [&] { xml.append("Last durations"); });
			Html::gen_table_body(xml, [&] (Xml_generator &xml) {
				Html::gen_table_key_value_row(xml, Html::String("Fetch"),
				                                   _last_fetch_duration);
				Html::gen_table_key_value_row(xml, Html::String("Extract"),
				                                   _last_extract_duration);
				Html::gen_table_key_value_row(xml, Html::String("Wipe"),
				                                   _last_wipe_duration);
				Html::gen_table_key_value_row(xml, Html::String("Generate"),
				                                   _last_generate_duration);
			});
		}

		Managed_init::with_cached_state_report([&] (Node const &node) {

			/* denote importing activity */
			if (node.has_sub_node("child"))
				xml.node("p", [&] {
					if (!_imports) xml.append("initial ");
					xml.append("import under way…"); });

			xml.node("h4", [&] { xml.append("init-state-report"); });
			xml.node("pre", [&] {
				xml.attribute("lang", "xml");
				xml.append_content(node); });
		}, [&] { });
	});
}


void Genodians::Import::state_update(Node const &state_node,
                                     bool        reconfigure_init)
{
	Seconds const current_secs = Seconds::from_rtc(_rtc.current_time());

	bool const timeout = _step_timeout_triggered;
	_step_timeout_triggered = false;

	State new_state      = State::INVALID;
	int   new_exit_value = -1;

	/*
	 * The following section evaluates the current state - even
	 * when a timeout happend because that is also treated like
	 * beginning the next step.
	 */

	switch (_state) {
	case State::INVALID:
	{
		break;
	}
	case State::INIT:
	{
		_import_start = current_secs;
		new_state = State::FETCH;
		break;
	}
	case State::FETCH:
	{
		new_state = _fetch->check(state_node).convert<State>(
			[&] (Managed_child::Ok ok) {
				return ok.finished ? State::WIPE : State::FETCH;
			},
			[&] (Managed_child::Error err) {
				new_exit_value = err.exit_value;
				return State::INVALID;
			});

		if (new_state != State::FETCH) {
			_fetch.destruct();
			if (new_state != State::INVALID)
				_last_fetch_duration =
					_import_step_start.diff(current_secs);
		}
		break;
	}
	case State::WIPE:
	{
		new_state = _wipe->check(state_node).convert<State>(
			[&] (Managed_child::Ok ok) {
				return ok.finished ? State::EXTRACT : State::WIPE;
			},
			[&] (Managed_child::Error err) {
				new_exit_value = err.exit_value;
				return State::INVALID;
			});

		if (new_state != State::WIPE) {
			_wipe.destruct();
			if (new_state != State::INVALID)
				_last_wipe_duration = _import_step_start.diff(current_secs);
		}
		break;
	}
	case State::EXTRACT:
	{
		new_state = _extract->check(state_node).convert<State>(
			[&] (Managed_child::Ok ok) {
				return ok.finished ? State::GENERATE : State::EXTRACT;
			},
			[&] (Managed_child::Error err) {
				new_exit_value = err.exit_value;
				return State::INVALID;
			});

		if (new_state != State::EXTRACT) {
			_extract.destruct();
			if (new_state != State::INVALID)
				_last_extract_duration = _import_step_start.diff(current_secs);
		}
		break;
	}
	case State::GENERATE:
	{
		new_state = _generate->check(state_node).convert<State>(
			[&] (Managed_child::Ok ok) {
				return ok.finished ? State::SLEEP : State::GENERATE;
			},
			[&] (Managed_child::Error err) {
				new_exit_value = err.exit_value;
				return State::INVALID;
			});

		if (new_state != State::GENERATE) {
			_generate.destruct();
			if (new_state != State::INVALID)
				_last_generate_duration = _import_step_start.diff(current_secs);
		}

		break;
	}
	case State::SLEEP:
	{
		new_state = _sleep_timeout_triggered ? State::INIT
		                                     : State::SLEEP;
		_sleep_timeout_triggered = false;
		break;
	}
	} /* switch */

	/* the step is still being processed without apparent problems  */
	if (_state == new_state && !reconfigure_init && !timeout)
		return;

	/* apply quota update */
	if (reconfigure_init) {
		Managed_init::generate_config([&] (Generator &g) {
			_update_init_config(g); });
		return;
	}

	/*
	 * Start next processing step or restart the current one in
	 * case the step-timeout triggered.
	 */

	if (!timeout || _step_timeout.scheduled())
		_step_timeout.discard();

	_import_step_start = current_secs;

	switch (new_state) {
	case State::FETCH:
	{
		if (timeout) _fetch->trigger_restart();
		else         _fetch.construct(Managed_init::child_states, _config.fetchurl);
		_step_timeout_secs = _calculate_timeout(_last_fetch_duration,
		                                        /*
		                                         * Failed downloads take up to 10s, so make
		                                         * room for the odd ones out to fail.
		                                         */
		                                        Seconds{.value = 60u});
		break;
	}
	case State::WIPE:
	{
		if (timeout) _wipe->trigger_restart();
		else         _wipe.construct(Managed_init::child_states, _config.wipe);
		_step_timeout_secs = _calculate_timeout(_last_wipe_duration);
		break;
	}
	case State::EXTRACT:
	{
		if (timeout) _extract->trigger_restart();
		else         _extract.construct(Managed_init::child_states, _config.extract);
		_step_timeout_secs = _calculate_timeout(_last_extract_duration);
		break;
	}
	case State::GENERATE:
	{
		if (timeout) _generate->trigger_restart();
		else         _generate.construct(Managed_init::child_states, _config.generate);
		_step_timeout_secs = _calculate_timeout(_last_generate_duration);
		break;
	}
	case State::SLEEP:
	{
		Microseconds const to_us =
			Microseconds { Microseconds(60'000'000ul).value * _config.sleep_duration };
		_sleep_timeout.schedule(to_us);
		_step_timeout_secs = { 0 };

		++_imports;
		Seconds const dur { .value = _config.sleep_duration * 60 };

		_import_duration = _import_start.diff(current_secs);

		_last_update = Utils::from_rtc(from_seconds(current_secs));
		_next_update = Utils::from_rtc(from_seconds({ current_secs.value + dur.value }));
		break;
	}
	default:
		/* INIT and INVALID are of no concern at this point */
		break;
	} /* switch */

	if (_step_timeout_secs.value) {
		Microseconds const to_us =
			Microseconds { Microseconds(1'000'000ul).value * _step_timeout_secs.value };
		_step_timeout.schedule(to_us);
	}

	// TODO evaluate new_exit_value to handle different invalid states

	_state = new_state;
	Managed_init::generate_config([&] (Generator &g) {
		_update_init_config(g); });
}


void Genodians::Import::_update_init_config(Generator &g)
{
	g.attribute("verbose", "no");

	g.node("report", [&] {
		g.attribute("init_ram",   "yes");
		g.attribute("init_caps",  "yes");
		g.attribute("child_ram",  "yes");
		g.attribute("child_caps", "yes");
		g.attribute("delay_ms",   5000);
		g.attribute("buffer",     "64K");
	});

	g.node("parent-provides", [&] {
		gen_parent_service<Rom_session>(g);
		gen_parent_service<Cpu_session>(g);
		gen_parent_service<Pd_session>(g);
		gen_parent_service<Rm_session>(g);
		gen_parent_service<Log_session>(g);
		gen_parent_service<Timer::Session>(g);
		gen_parent_service<Rtc::Session>(g);
		gen_parent_service<Nic::Session>(g);
		gen_parent_service<File_system::Session>(g);
	});

	gen_heartbeat_node(g, _config.heartbeat_ms);

	gen_default_route_parent(g);

	if (_fetch.constructed())    _fetch->   generate(g);
	if (_wipe.constructed())     _wipe->    generate(g);
	if (_extract.constructed())  _extract-> generate(g);
	if (_generate.constructed()) _generate->generate(g);
}


struct Genodians::Lighttpd : Genodians::Managed_init
{
	Env &_env;

	Rtc::Connection &_rtc;

	void _update_init_config(Generator &);

	Config::Lighttpd const &_config;

	Child_state _child_state;

	Date     _last_restart { };
	unsigned _restarts = 0;

	void _restart()
	{
		_restarts++;
		_last_restart = from_rtc(_rtc.current_time());

		Managed_init::generate_config([&] (Generator &g) {
			_update_init_config(g); });
	}

	Lighttpd(Env                    &env,
	         Allocator              &alloc,
	         Notify_interface       &notify,
	         Rtc::Connection        &rtc,
	         Config::Lighttpd const &config)
	:
		Managed_init  { env, alloc, notify,
		                "lighttpd.state", "lighttpd.config" },
		_env          { env },
		_rtc          { rtc },
		_config       { config },
		_child_state  { Managed_init::child_states, "lighttpd",
		                Priority { 0 }, _config.lighttpd.ram,
		                                _config.lighttpd.caps }
	{
		/* initial init configuration */
		Managed_init::generate_config([&] (Generator &g) {
			_update_init_config(g); });
	}

	void trigger_restart()
	{
		_child_state.trigger_restart();
		_restart();
	}

	/****************************
	 ** Managed_init interface **
	 ****************************/

	void generate_report(Xml_generator &xml)      const override;
	void state_update   (Node const &, bool )     override;
};


void Genodians::Lighttpd::generate_report(Xml_generator &xml) const
{
	Html::gen_section_div(xml, "Lighttpd", [&] (Xml_generator &xml) {
		Html::gen_table_body(xml, [&] (Xml_generator &xml) {
			Html::gen_table_key_value_row(xml, Html::String("Total restarts"),
			                                   Html::String(_restarts));
			if (_restarts == 0)
				return;

			Html::gen_table_key_value_row(xml, Html::String("Last restart"),
			                                   _last_restart);
		});
		Managed_init::with_cached_state_report([&] (Node const &node) {
			xml.node("h4", [&] { xml.append("init-state-report"); });
			xml.node("pre", [&] {
				xml.attribute("lang", "xml");
				xml.append_content(node); });
		}, [&] { });
	});
}


void Genodians::Lighttpd::state_update(Node const &state_node,
                                       bool        reconfigure_init)
{
	Child_exit_state const exit_state(state_node, "lighttpd");

	reconfigure_init |= exit_state.exited || !exit_state.responsive;

	if (!reconfigure_init)
		return;

	_restart();
}


void Genodians::Lighttpd::_update_init_config(Generator &g)
{
	g.attribute("verbose", "no");

	g.node("report", [&] {
		g.attribute("init_ram",   "yes");
		g.attribute("init_caps",  "yes");
		g.attribute("child_ram",  "yes");
		g.attribute("child_caps", "yes");
		g.attribute("delay_ms",   5000);
		g.attribute("buffer",     "64K");
	});

	g.node("parent-provides", [&] {
		gen_parent_service<Rom_session>(g);
		gen_parent_service<Cpu_session>(g);
		gen_parent_service<Pd_session>(g);
		gen_parent_service<Rm_session>(g);
		gen_parent_service<Log_session>(g);
		gen_parent_service<Timer::Session>(g);
		gen_parent_service<Rtc::Session>(g);
		gen_parent_service<Nic::Session>(g);
		gen_parent_service<File_system::Session>(g);
	});

	gen_heartbeat_node(g, _config.heartbeat_ms);

	g.node("start", [&] {
		_child_state.gen_start_node_content(g);

		g.node("heartbeat", [&] { });

		g.node("config", [&] {
			g.attribute("ld_verbose", "yes");

			gen_arg(g, "lighttpd");
			gen_arg(g, "-f");
			gen_arg(g, "/etc/lighttpd/lighttpd.conf");
			gen_arg(g, "-D");

			g.node("vfs", [&] {

				gen_named_dir(g, "dev", [&] (Generator &g) {
					g.node("log",  [&] { });
					g.node("null", [&] { });
					g.node("rtc",  [&] { });
					g.node("jitterentropy", [&] { g.attribute("name", "random"); }); });

				gen_named_dir(g, "socket", [&] (Generator &g) {
					g.node("lxip", [&] { g.attribute("dhcp", "yes"); }); });

				gen_named_dir(g, "etc", [&] (Generator &g) {
					gen_named_dir(g, "lighttpd", [&] (Generator &g) {
						gen_rom(g, "lighttpd.conf");
						gen_rom(g, "upload-user.conf");
						g.node("fs", [&] { g.attribute("label", "cert"); }); }); });

				gen_named_dir(g, "website", [&] (Generator &g) {
					gen_named_dir(g, ".well-known", [&] (Generator &g) {
						gen_symlink(g, "acme-challenge", "/upload/acme-challenge"); });
					gen_symlink(g, "upload", "/upload");
					g.node("fs", [&] { g.attribute("label", "website"); }); });

				gen_named_dir(g, "upload", [&] (Generator &g) {
					gen_symlink(g, "cert", "/etc/lighttpd/public");
					gen_named_dir(g, "acme-challenge", [] (Generator &g) {
						g.node("ram"); }); });

			});

			g.node("libc", [&] {
				g.attribute("stdin",  "/dev/null");
				g.attribute("stdout", "/dev/log");
				g.attribute("stderr", "/dev/log");
				g.attribute("rtc",    "/dev/rtc");
				g.attribute("socket", "/socket"); }); });

		g.node("route", [&] {
			gen_service_node<File_system::Session>(g, [&] {
				g.attribute("label", "cert");
				g.node("parent", [&] {
					g.attribute("label", "cert"); }); });
			gen_service_node<File_system::Session>(g, [&] {
				g.attribute("label", "website");
				g.node("parent", [&] {
					g.attribute("label", "website"); }); });
			gen_service_node<Nic::Session>(g, [&] {
				g.node("parent", [&] { }); });
			gen_parent_route<Cpu_session>   (g);
			gen_parent_route<Log_session>   (g);
			gen_parent_route<Pd_session>    (g);
			gen_parent_route<Rom_session>   (g);
			gen_parent_route<Rtc::Session>  (g);
			gen_parent_route<Timer::Session>(g);
		});
	});
}


struct Genodians::Main
{
	Env  &_env;
	Heap  _heap { _env.ram(), _env.rm() };

	Timer::Connection _timer;
	Rtc::Connection   _rtc;

	Attached_rom_dataspace _config_rom {
		_env, "config" };

	Config _update_from_config_rom()
	{
		_config_rom.update();
		return Config::update_from_node(_config_rom.node());
	}

	Config _config;

	Lighttpd _lighttpd;
	Import   _import;

	Timer::One_shot_timeout<Main> _fullchain_update_timeout {
		_timer, *this, &Main::_handle_fullchain_update_timeout };

	void _handle_fullchain_update_timeout(Duration)
	{
		log("Certificate updated, restart lighttpd");
		_lighttpd.trigger_restart();
	}

	Attached_rom_dataspace _fullchain_rom {
		_env, "fullchain.pem" };

	Signal_handler<Main> _fullchain_rom_sigh {
		_env.ep(), *this, &Main::_handle_fullchain_update };

	void _handle_fullchain_update()
	{
		/*
		 * The 'fullchain.pem' file is at the moment monitored via
		 * the fs_query component that watches for changes in the
		 * 'public' directory. The interaction of lighttpd with
		 * this directory during uploads can generate multiple
		 * reports and at the some time restarting lighttpd at the
		 * wrong time can lead to prematurely cutting the connection
		 * while the remote side is still waiting for an response.
		 *
		 * To mitigate this situation defer the restart of lighttpd
		 * by some seconds.
		 */
		Microseconds const schedule_restart = Microseconds {
			1'000'000ull * 5 };
		_fullchain_update_timeout.schedule(schedule_restart);
	}

	Expanding_reporter _status_reporter {
		_env, "html", "status.html" };

	Signal_handler<Main> _status_sigh {
		_env.ep(), *this, &Main::_handle_status };

	Timer::One_shot_timeout<Main> _status_timeout {
		_timer, *this, &Main::_handle_status_timeout };

	void _handle_status_timeout(Duration) {
		_handle_status(); }

	struct State_rom_handler
	{
		Attached_rom_dataspace            _rom;
		Signal_handler<State_rom_handler> _sigh;

		void _handle() { _rom.update(); }

		State_rom_handler(Env &env, char const *rom_name)
		:
			_rom  { env, rom_name },
			_sigh { env.ep(), *this, &State_rom_handler::_handle }
		{
			_rom.sigh(_sigh);
			_handle();
		}

		void with_node(auto const &fn) {
			if (_rom.valid()) fn(_rom.node()); }
	};

	State_rom_handler _nic_router_state_rom;

	void _generate_nic_router_report(Xml_generator &xml, Node const &node)
	{
		auto td_centered = [&] (Xml_generator &xml, auto value) {
			xml.node("td", [&] {
				xml.attribute("style", "text-align:center");
				xml.append(value); });
		};

		auto td_right = [&] (Xml_generator &xml, auto value) {
			xml.node("td", [&] {
				xml.attribute("style", "text-align:right");
				xml.append_sanitized(String<48>{value}); });
		};

		xml.node("table", [&] {
			xml.node("thead", [&] {
				xml.node("tr", [&] {
					td_centered(xml, "Traffic");
					td_centered(xml, "Rx");
					td_centered(xml, "Tx");
				}); });

			xml.node("tbody", [&] {
				node.for_each_sub_node("domain", [&] (Node const &domain) {
					xml.node("tr", [&] {
						xml.node("td", [&] {
							xml.append_sanitized(domain.attribute_value("name", Html::String())); });
						td_right(xml, domain.attribute_value("rx_bytes", 0ull));
						td_right(xml, domain.attribute_value("tx_bytes", 0ull));
					}); }); });
		});
	}

	Utils::Seconds _uptime { 0 };
	Date           _last_status_update { };

	void _generate_report(Xml_generator &xml)
	{
		Html::gen_section_div(xml, "Overview", [&] (Xml_generator &xml) {
			Html::gen_table_body(xml, [&] (Xml_generator &xml) {
				Html::gen_table_key_value_row(xml,
					Html::String("Last update"), _last_status_update);

				Html::gen_table_key_value_row(xml,
					Html::String("Uptime"), Html::String(_uptime));

				_nic_router_state_rom.with_node([&] (Node const &node) {
					_generate_nic_router_report(xml, node);
				});
			});
		});
	}

	void _handle_status()
	{
		_last_status_update = from_rtc(_rtc.current_time());

		_uptime = { _timer.curr_time().trunc_to_plain_ms().value / 1'000u };

		_status_reporter.generate_xml([&] (Xml_generator &xml) {

			xml.node("head", [&] {
				xml.node("meta", [&] {
					xml.attribute("charset", "utf-8"); });
				xml.node("title", [&] {
					xml.append("Genodians Status Overview"); });
			});

			xml.node("body", [&] {

				xml.attribute("style", "font-family:monospace");

				_generate_report(xml);

				_lighttpd.generate_report(xml);
				_import.  generate_report(xml);
			});
		});

		/*
		 * Make sure the status is updated once in a while in case
		 * the init-state reports are triggered sparingly.
		 */
		Microseconds const next_update = Microseconds {
			1'000'000ull * _config.status_update_interval };
		_status_timeout.schedule(next_update);
	}

	struct Status_notifier : Notify_interface
	{
		Signal_handler<Main> &_handler;

		Status_notifier(Signal_handler<Main> &handler)
		: _handler { handler } { }

		void notify() const override {
			_handler.local_submit(); }
	};

	Status_notifier _status_notifier { _status_sigh };

	Rom_handler<Main> _fetch_lighttpd_handler;

	void _handle_fetch_lighttpd(Node const &node)
	{
		static constexpr unsigned MAX_CHECKS = 3;
		static unsigned check_count = 0;

		/* inhibit check when we have not yet imported anything */
		if (!_import._imports) {
			log("Inhibit lighttpd check");
			return;
		}

		auto check_progress = [&] (Node const &fetch_node) {
			/* ignore transient reports */
			if (!fetch_node.attribute_value("finished", false))
				return;

			using Result = String<16>;

			Result const result = fetch_node.attribute_value("result", Result());
			/* never happens */
			if (!result.valid())
				return;

			if (result == "failed")
				++check_count;
			else if (result == "success")
				check_count = 0;

			if (check_count) {
				log("Lighttpd check already failed ", check_count, " times");

				_nic_router_state_rom.with_node([&] (Node const &node) {
					log(node);
				});
			}

			if (check_count >= MAX_CHECKS) {
				/* XXX consider certificate update */
				log("Restart lighttpd as the check failed repeatedly");
				_lighttpd.trigger_restart();
				check_count = 0;
			}
		};
		node.with_optional_sub_node("fetch", check_progress);
	}

	Main(Env &env)
	:
		_env      { env },
		_timer    { _env },
		_rtc      { _env },
		_config   { _update_from_config_rom() },
		_lighttpd { _env, _heap, _status_notifier, _rtc,
		                         _config.lighttpd_config },
		_import   { _env, _heap, _status_notifier, _timer, _rtc,
		                         _config.import_config },
		_nic_router_state_rom { _env, "nic_router.state" },
		_fetch_lighttpd_handler { _env, "fetch_lighttpd.report", *this,
		                          &Main::_handle_fetch_lighttpd }
	{
		_fullchain_rom.sigh(_fullchain_rom_sigh);

		/* trigger initial status report */
		_handle_status();
	}
};


void Component::construct(Genode::Env &env)
{
	static Genodians::Main main(env);
}
