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
	void gen_default_route_parent(Xml_generator &xml)
	{
		xml.node("default-route", [&] {
			xml.node("any-service", [&] {
				xml.node("parent", [&] { }); }); });
	}

	void gen_common_parent_routes(Xml_generator &xml)
	{
		gen_service_node<Rom_session>(xml, [&] { xml.node("parent", [&] {}); });
		gen_service_node<Cpu_session>(xml, [&] { xml.node("parent", [&] {}); });
		gen_service_node<Pd_session> (xml, [&] { xml.node("parent", [&] {}); });
		gen_service_node<Rm_session> (xml, [&] { xml.node("parent", [&] {}); });
		gen_service_node<Log_session>(xml, [&] { xml.node("parent", [&] {}); });
	}

	void gen_arg(Xml_generator &xml, auto const &arg) {
		xml.node("arg", [&] { xml.attribute("value", arg); }); }

	void gen_named_dir(Xml_generator &xml, char const *name, auto const &fn)
	{
		xml.node("dir", [&] {
			xml.attribute("name", name);
			fn(xml); });
	}

	void gen_symlink(Xml_generator &xml, char const *name, char const *target)
	{
		xml.node("symlink", [&] {
			xml.attribute("name", name);
			xml.attribute("target", target); });
	}

	void gen_rom(Xml_generator &xml, char const *name)
	{
		xml.node("rom", [&] {
			xml.attribute("name", name);
			xml.attribute("binary", "no"); });
	}

	void gen_heartbeat_node(Xml_generator &xml, unsigned rate_ms) {
		xml.node("heartbeat", [&] { xml.attribute("rate_ms", rate_ms); }); }
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

		bool _evalute_child_states(Xml_node const &state)
		{
			/* upgrade RAM and cap quota on demand */
			bool reconfigure = false;
			state.for_each_sub_node("child", [&] (Xml_node child) {
				bool reconfiguration_needed = false;
				child_states.for_each([&] (Child_state &child_state) {
					if (child_state.apply_child_state_report(child))
						reconfiguration_needed = true; });
				if (reconfiguration_needed)
					reconfigure = true;
			});
			return reconfigure;
		}

		Constructible<Buffered_xml> _cached_state_report { };

		bool _update_cached_state_report(Xml_node const &node)
		{
			_cached_state_report.construct(_alloc, node);
			return true;
		}

		Rom_handler<Managed_init> _state_rom;

		void _state_update(Xml_node const &node) {

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
			_config_reporter.generate([&] (Xml_generator &xml) {
				fn(xml); });
		}

		void with_cached_state_report(auto const &avail_fn,
		                              auto const &missing_fn) const
		{
			if (_cached_state_report.constructed())
				_cached_state_report->with_xml_node(
					[&] (Xml_node const &node) { avail_fn(node); });
			else
				missing_fn();
		}

		/****************************
		 ** Managed_init interface **
		 ****************************/

		virtual void generate_report(Xml_generator &)        const = 0;
		virtual void state_update   (Xml_node const &, bool)       = 0;
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

		Result check(Xml_node const &state_node)
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

		void gen_start_node_content(Xml_generator &xml) const {
			_child_state.gen_start_node_content(xml); }

		void trigger_restart() {
			_child_state.trigger_restart(); }

		Start_name const name() const {
			return _child_state.name(); }

		/*****************************
		 ** Managed_child interface **
		 *****************************/

		virtual void  generate(Xml_generator &xml) const = 0;
	};

	struct Fetch;
	struct Wipe;
	struct Extract;
	struct Generate;

	auto with_sub_node(Xml_node const &node,
	                   char     const *sub_node_name,
	                   auto     const &fn)
	{
		if (node.has_sub_node(sub_node_name))
			return fn(node.sub_node(sub_node_name));

		static Xml_node const empty_node("<empty/>");
		return fn(empty_node);
	}

	struct Config
	{
		struct Child
		{
			Ram_quota ram;
			Cap_quota caps;

			static Child from_xml(Xml_node const &node)
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

		static Config update_from_xml(Xml_node const &config_node)
		{
			unsigned const status_update_interval =
				config_node.attribute_value("status_update_interval_sec", 60u);

			Xml_node const &lighttpd_node =
				config_node.has_sub_node("lighttpd") ? config_node.sub_node("import")
			                                         : Xml_node("<empty/>");
			Child const lighttpd =
				with_sub_node(config_node, "lighttpd", [&] (Xml_node const &node) {
					return Child::from_xml(node); });
			unsigned const lighttpd_heartbeat_ms =
				lighttpd_node.attribute_value("heartbeat_ms", 3000u);

			Xml_node const &import_node =
				config_node.has_sub_node("import") ? config_node.sub_node("import")
			                                       : Xml_node("<empty/>");
			unsigned const import_heartbeat_ms =
				import_node.attribute_value("heartbeat_ms", 3000u);
			unsigned const sleep_duration =
				import_node.attribute_value("update_interval_min", 180u);
			Child const fetchurl =
				with_sub_node(import_node, "fetchurl", [&] (Xml_node const &node) {
					return Child::from_xml(node); });
			Child const wipe =
				with_sub_node(import_node, "wipe", [&] (Xml_node const &node) {
					return Child::from_xml(node); });
			Child const extract =
				with_sub_node(import_node, "extract", [&] (Xml_node const &node) {
					return Child::from_xml(node); });
			Child const generate =
				with_sub_node(import_node, "generate", [&] (Xml_node const &node) {
					return Child::from_xml(node); });

			return Config {
				.status_update_interval = status_update_interval,
				.lighttpd_config = Lighttpd {
					.lighttpd     = lighttpd,
					.heartbeat_ms = lighttpd_heartbeat_ms
				},
				.import_config   = Import {
					.fetchurl       = fetchurl,
					.wipe           = wipe,
					.extract        = extract,
					.generate       = generate,
					.sleep_duration = sleep_duration,
					.heartbeat_ms   = import_heartbeat_ms
				}
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

	void generate(Xml_generator &xml) const override
	{
		xml.node("start", [&] {
			gen_start_node_content(xml);

			xml.node("heartbeat", [&] { });

			xml.node("route", [&] {
				gen_service_node<Rom_session>(xml, [&] {
					xml.attribute("label", "config");
					xml.node("parent", [&] {
						xml.attribute("label", "fetchurl.config"); }); });
				gen_service_node<Nic::Session>(xml, [&] {
					xml.node("parent", [&] { }); });
				gen_service_node<File_system::Session>(xml, [&] {
					xml.node("parent", [&] { }); });
				gen_service_node<Timer::Session>(xml, [&] {
					xml.node("parent", [&] { }); });
				gen_common_parent_routes(xml);
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

	void generate(Xml_generator &xml) const override
	{
		xml.node("start", [&] {
			gen_start_node_content(xml);
			gen_named_node(xml, "binary", "init");

			xml.node("heartbeat", [&] { });

			xml.node("route", [&] {
				gen_service_node<Rom_session>(xml, [&] {
					xml.attribute("label", "config");
					xml.node("parent", [&] {
						xml.attribute("label", "wipe.config"); }); });
				gen_service_node<File_system::Session>(xml, [&] {
					xml.node("parent", [&] { }); });
				gen_service_node<Timer::Session>(xml, [&] {
					xml.node("parent", [&] { }); });
				gen_common_parent_routes(xml);
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

	void generate(Xml_generator &xml) const override
	{
		xml.node("start", [&] {
			gen_start_node_content(xml);

			xml.node("heartbeat", [&] { });

			xml.node("route", [&] {
				gen_service_node<File_system::Session>(xml, [&] {
					xml.node("parent", [&] { }); });
				gen_service_node<Rom_session>(xml, [&] {
					xml.attribute("label", "config");
					xml.node("parent", [&] {
						xml.attribute("label", "extract.config"); }); });
				gen_common_parent_routes(xml);
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

	void generate(Xml_generator &xml) const override
	{
		xml.node("start", [&] {
			gen_start_node_content(xml);

			gen_named_node(xml, "binary", "init");
			xml.node("heartbeat", [&] { });

			xml.node("route", [&] {
				gen_service_node<Rom_session>(xml, [&] {
					xml.attribute("label", "config");
					xml.node("parent", [&] {
						xml.attribute("label", "generate.config"); }); });
				gen_service_node<File_system::Session>(xml, [&] {
					xml.node("parent", [&] { }); });
				gen_service_node<Timer::Session>(xml, [&] {
					xml.node("parent", [&] { }); });
				gen_service_node<Rtc::Session>(xml, [&] {
					xml.node("parent", [&] { }); });
				gen_common_parent_routes(xml);
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

		with_cached_state_report([&] (Xml_node const &node) {
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

		with_cached_state_report([&] (Xml_node const &node) {
			state_update(node, false); }, [&] { });
	}

	void _update_init_config(Xml_generator &xml);

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
	void state_update   (Xml_node const &, bool)       override;
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

		Managed_init::with_cached_state_report([&] (Xml_node const &node) {

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


void Genodians::Import::state_update(Xml_node const &state_node,
                                     bool            reconfigure_init)
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
		Managed_init::generate_config([&] (Xml_generator &xml) {
			_update_init_config(xml); });
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
	Managed_init::generate_config([&] (Xml_generator &xml) {
		_update_init_config(xml); });
}


void Genodians::Import::_update_init_config(Xml_generator &xml)
{
	xml.attribute("verbose", "no");

	xml.node("report", [&] {
		xml.attribute("init_ram",   "yes");
		xml.attribute("init_caps",  "yes");
		xml.attribute("child_ram",  "yes");
		xml.attribute("child_caps", "yes");
		xml.attribute("delay_ms",   5000);
		xml.attribute("buffer",     "64K");
	});

	xml.node("parent-provides", [&] {
		gen_parent_service<Rom_session>(xml);
		gen_parent_service<Cpu_session>(xml);
		gen_parent_service<Pd_session>(xml);
		gen_parent_service<Rm_session>(xml);
		gen_parent_service<Log_session>(xml);
		gen_parent_service<Timer::Session>(xml);
		gen_parent_service<Rtc::Session>(xml);
		gen_parent_service<Nic::Session>(xml);
		gen_parent_service<File_system::Session>(xml);
	});

	gen_heartbeat_node(xml, _config.heartbeat_ms);

	gen_default_route_parent(xml);

	if (_fetch.constructed())    _fetch->   generate(xml);
	if (_wipe.constructed())     _wipe->    generate(xml);
	if (_extract.constructed())  _extract-> generate(xml);
	if (_generate.constructed()) _generate->generate(xml);
}


struct Genodians::Lighttpd : Genodians::Managed_init
{
	Env &_env;

	Rtc::Connection &_rtc;

	void _update_init_config(Xml_generator &);

	Config::Lighttpd const &_config;

	Child_state _child_state;

	Date     _last_restart { };
	unsigned _restarts = 0;

	void _restart()
	{
		_restarts++;
		_last_restart = from_rtc(_rtc.current_time());

		Managed_init::generate_config([&] (Xml_generator &xml) {
			_update_init_config(xml); });
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
		Managed_init::generate_config([&] (Xml_generator &xml) {
			_update_init_config(xml); });
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
	void state_update   (Xml_node const &, bool )       override;
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
		Managed_init::with_cached_state_report([&] (Xml_node const &node) {
			xml.node("h4", [&] { xml.append("init-state-report"); });
			xml.node("pre", [&] {
				xml.attribute("lang", "xml");
				xml.append_content(node); });
		}, [&] { });
	});
}


void Genodians::Lighttpd::state_update(Xml_node const &state_node,
                                       bool            reconfigure_init)
{
	Child_exit_state const exit_state(state_node, "lighttpd");

	reconfigure_init |= exit_state.exited || !exit_state.responsive;

	if (!reconfigure_init)
		return;

	_restart();
}


void Genodians::Lighttpd::_update_init_config(Xml_generator &xml)
{
	xml.attribute("verbose", "no");

	xml.node("report", [&] {
		xml.attribute("init_ram",   "yes");
		xml.attribute("init_caps",  "yes");
		xml.attribute("child_ram",  "yes");
		xml.attribute("child_caps", "yes");
		xml.attribute("delay_ms",   5000);
		xml.attribute("buffer",     "64K");
	});

	xml.node("parent-provides", [&] {
		gen_parent_service<Rom_session>(xml);
		gen_parent_service<Cpu_session>(xml);
		gen_parent_service<Pd_session>(xml);
		gen_parent_service<Rm_session>(xml);
		gen_parent_service<Log_session>(xml);
		gen_parent_service<Timer::Session>(xml);
		gen_parent_service<Rtc::Session>(xml);
		gen_parent_service<Nic::Session>(xml);
		gen_parent_service<File_system::Session>(xml);
	});

	gen_heartbeat_node(xml, _config.heartbeat_ms);

	xml.node("start", [&] {
		_child_state.gen_start_node_content(xml);

		xml.node("heartbeat", [&] { });

		xml.node("config", [&] {
			xml.attribute("ld_verbose", "yes");

			gen_arg(xml, "lighttpd");
			gen_arg(xml, "-f");
			gen_arg(xml, "/etc/lighttpd/lighttpd.conf");
			gen_arg(xml, "-D");

			xml.node("vfs", [&] {

				gen_named_dir(xml, "dev", [&] (Xml_generator &xml) {
					xml.node("log",  [&] { });
					xml.node("null", [&] { });
					xml.node("rtc",  [&] { });
					xml.node("jitterentropy", [&] { xml.attribute("name", "random"); }); });

				gen_named_dir(xml, "socket", [&] (Xml_generator &xml) {
					xml.node("lxip", [&] { xml.attribute("dhcp", "yes"); }); });

				gen_named_dir(xml, "etc", [&] (Xml_generator &xml) {
					gen_named_dir(xml, "lighttpd", [&] (Xml_generator &xml) {
						gen_rom(xml, "lighttpd.conf");
						gen_rom(xml, "upload-user.conf");
						xml.node("fs", [&] { xml.attribute("label", "cert"); }); }); });

				gen_named_dir(xml, "website", [&] (Xml_generator &xml) {
					gen_named_dir(xml, ".well-known", [&] (Xml_generator &xml) {
						gen_symlink(xml, "acme-challenge", "/upload/acme-challenge"); });
					gen_symlink(xml, "upload", "/upload");
					xml.node("fs", [&] { xml.attribute("label", "website"); }); });

				gen_named_dir(xml, "upload", [&] (Xml_generator &xml) {
					gen_symlink(xml, "cert", "/etc/lighttpd/public");
					gen_named_dir(xml, "acme-challenge", [] (Xml_generator &xml) {
						xml.node("ram"); }); });

			});

			xml.node("libc", [&] {
				xml.attribute("stdin",  "/dev/null");
				xml.attribute("stdout", "/dev/log");
				xml.attribute("stderr", "/dev/log");
				xml.attribute("rtc",    "/dev/rtc");
				xml.attribute("socket", "/socket"); }); });

		xml.node("route", [&] {
			gen_service_node<File_system::Session>(xml, [&] {
				xml.attribute("label", "cert");
				xml.node("parent", [&] {
					xml.attribute("label", "cert"); }); });
			gen_service_node<File_system::Session>(xml, [&] {
				xml.attribute("label", "website");
				xml.node("parent", [&] {
					xml.attribute("label", "website"); }); });
			gen_service_node<Nic::Session>(xml, [&] {
				xml.node("parent", [&] { }); });
			gen_parent_route<Cpu_session>   (xml);
			gen_parent_route<Log_session>   (xml);
			gen_parent_route<Pd_session>    (xml);
			gen_parent_route<Rom_session>   (xml);
			gen_parent_route<Rtc::Session>  (xml);
			gen_parent_route<Timer::Session>(xml);
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
		return Config::update_from_xml(_config_rom.xml());
	}

	Config _config;

	Lighttpd _lighttpd;
	Import   _import;

	Attached_rom_dataspace _fullchain_rom {
		_env, "fullchain.pem" };

	Signal_handler<Main> _fullchain_rom_sigh {
		_env.ep(), *this, &Main::_handle_fullchain_update };

	void _handle_fullchain_update()
	{
		log("Certificate updated, restart lighttpd");
		_lighttpd.trigger_restart();
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

		void with_xml(auto const &fn) {
			if (_rom.valid()) fn(_rom.xml()); }
	};

	State_rom_handler _nic_router_state_rom;

	void _generate_nic_router_report(Xml_generator &xml, Xml_node const &node)
	{
		auto td_centered = [&] (Xml_generator &xml, auto value) {
			xml.node("td", [&] {
				xml.attribute("style", "text-align:center");
				xml.append(value); });
		};

		auto td_right = [&] (Xml_generator &xml, auto value) {
			xml.node("td", [&] {
				xml.attribute("style", "text-align:right");
				xml.append_content(value); });
		};

		xml.node("table", [&] {
			xml.node("thead", [&] {
				xml.node("tr", [&] {
					td_centered(xml, "Traffic");
					td_centered(xml, "Rx");
					td_centered(xml, "Tx");
				}); });

			xml.node("tbody", [&] {
				node.for_each_sub_node("domain", [&] (Xml_node const &domain) {
					xml.node("tr", [&] {
						xml.node("td", [&] {
							xml.append_content(domain.attribute_value("name", Html::String())); });
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

				_nic_router_state_rom.with_xml([&] (Xml_node const &node) {
					_generate_nic_router_report(xml, node);
				});
			});
		});
	}

	void _handle_status()
	{
		_last_status_update = from_rtc(_rtc.current_time());

		_uptime = { _timer.curr_time().trunc_to_plain_ms().value / 1'000u };

		_status_reporter.generate([&] (Xml_generator &xml) {

			xml.node("head", [&] {
				xml.node("meta", [&] {
					xml.attribute("charset", "utf-8"); });
				xml.node("title", [&] {
					xml.append("Genodians Status Overview"); });
			});

			xml.node("body", [&] {

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

	void _handle_fetch_lighttpd(Xml_node const &node)
	{
		static constexpr unsigned MAX_CHECKS = 3;
		static unsigned check_count = 0;

		/* inhibit check when we have not yet imported anything */
		if (!_import._imports) {
			log("Inhibit lighttpd check");
			return;
		}

		auto check_progress = [&] (Xml_node const &fetch_node) {
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

				_nic_router_state_rom.with_xml([&] (Xml_node const &node) {
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
