/*
 *	A Spin box widget
 *	Nana C++ Library(http://www.nanapro.org)
 *	Copyright(C) 2003-2015 Jinhao(cnjinhao@hotmail.com)
 *
 *	Distributed under the Boost Software License, Version 1.0.
 *	(See accompanying file LICENSE_1_0.txt or copy at
 *	http://www.boost.org/LICENSE_1_0.txt)
 *
 *	@file: nana/gui/widgets/spanbox.cpp
 */

#include <nana/gui/widgets/spinbox.hpp>
#include <nana/gui/widgets/skeletons/text_editor.hpp>
#include <nana/gui/element.hpp>
#include <nana/gui/timer.hpp>

namespace nana
{
	namespace drawerbase
	{
		namespace spinbox
		{
			enum class buttons
			{
				none, increase, decrease
			};

			class range_interface
			{
			public:
				virtual ~range_interface() = default;

				virtual std::wstring value() const = 0;
				virtual void spin(bool increase) = 0;
			};

			template<typename T>
			class range_numeric
				: public range_interface
			{
			public:
				range_numeric(T vbegin, T vlast, T step)
					: begin_{ vbegin }, last_{ vlast }, step_{ step }, value_{ vbegin }
				{}

				std::wstring value() const override
				{
					std::wstringstream ss;
					ss << value_;
					return ss.str();
				}

				void spin(bool increase) override
				{
					if (increase)
					{
						value_ += step_;
						if (value_ > last_)
							value_ = last_;
					}
					else
					{
						value_ -= step_;
						if (value_ < begin_)
							value_ = begin_;
					}
				}
			private:
				T begin_;
				T last_;
				T step_;
				T value_;
			};

			class range_text
				: public range_interface
			{
			public:
				range_text(std::initializer_list<std::string> & initlist)
				{
					for (auto & s : initlist)
					{
						texts_.emplace_back(::nana::charset(s, ::nana::unicode::utf8));
					}
				}

				range_text(std::initializer_list<std::wstring>& initlist)
					: texts_(initlist)
				{}

				std::wstring value() const override
				{
					if (texts_.empty())
						return{};

					return texts_[pos_];
				}

				void spin(bool increase) override
				{
					if (texts_.empty())
						return;

					if (increase)
					{
						++pos_;
						if (texts_.size() <= pos_)
							pos_ = texts_.size() - 1;
					}
					else
					{
						--pos_;
						if (texts_.size() <= pos_)
							pos_ = 0;
					}
				}
			private:
				std::vector<std::wstring> texts_;
				std::size_t pos_{0};
			};

			class implementation
			{
			public:
				implementation()
				{
					//Sets a timer for continous spin when mouse button is pressed.
					timer_.elapse([this]
					{
						range_->spin(buttons::increase == spin_stated_);
						_m_text();
						API::update_window(editor_->window_handle());

						auto intv = timer_.interval();
						if (intv > 50)
							timer_.interval(intv / 2);
					});

					timer_.interval(1000);
				}

				void attach(::nana::widget& wdg, ::nana::paint::graphics& graph)
				{
					auto wd = wdg.handle();
					graph_ = &graph;
					auto scheme = static_cast<::nana::widgets::skeletons::text_editor_scheme*>(API::dev::get_scheme(wd));
					editor_ = new ::nana::widgets::skeletons::text_editor(wd, graph, scheme);
					editor_->multi_lines(false);

					if (!range_)
						range_.reset(new range_numeric<int>(0, 100, 1));

					_m_text();

					API::tabstop(wd);
					API::eat_tabstop(wd, true);
					API::effects_edge_nimbus(wd, effects::edge_nimbus::active);
					API::effects_edge_nimbus(wd, effects::edge_nimbus::over);
					_m_reset_text_area();
				}

				void detach()
				{
					delete editor_;
					editor_ = nullptr;
				}

				void set_range(std::unique_ptr<range_interface> ptr)
				{
					range_.swap(ptr);

					_m_text();
				}

				void qualify(std::wstring&& prefix, std::wstring&& suffix)
				{
					surround_.prefix = std::move(prefix);
					surround_.suffix = std::move(suffix);

					if (editor_)
					{
						_m_text();
						API::update_window(editor_->window_handle());
					}
				}

				void render()
				{
					editor_->render(API::is_focus_window(editor_->window_handle()));
					_m_draw_spins(spin_stated_);
				}

				::nana::widgets::skeletons::text_editor* editor() const
				{
					return editor_;
				}

				void mouse_wheel(bool upwards)
				{
					range_->spin(!upwards);
					_m_text();
				}

				bool mouse_button(const ::nana::arg_mouse& arg, bool pressed)
				{
					if (!pressed)
					{
						API::capture_window(editor_->window_handle(), false);
						timer_.stop();
						timer_.interval(1000);
					}

					if (buttons::none != spin_stated_)
					{
						//Spins the value when mouse button is released
						if (pressed)
						{
							API::capture_window(editor_->window_handle(), true);
							range_->spin(buttons::increase == spin_stated_);
							_m_text();
							timer_.start();
						}
						_m_draw_spins(spin_stated_);
						return true;
					}


					bool refreshed = false;
					if (pressed)
						refreshed = editor_->mouse_down(arg.left_button, arg.pos);
					else
						refreshed = editor_->mouse_up(arg.left_button, arg.pos);

					if (refreshed)
						_m_draw_spins(buttons::none);

					return refreshed;
				}

				bool mouse_move(bool left_button, const ::nana::point& pos)
				{
					if (editor_->mouse_move(left_button, pos))
					{
						editor_->reset_caret();
						render();
						return true;
					}

					auto btn = _m_where(pos);
					if (buttons::none != btn)
					{
						spin_stated_ = btn;
						_m_draw_spins(btn);
						return true;
					}
					else if (buttons::none != spin_stated_)
					{
						spin_stated_ = buttons::none;
						_m_draw_spins(buttons::none);
						return true;
					}
					
					return false;
				}
			private:
				void _m_text()
				{
					if (editor_)
					{
						std::wstring text = surround_.prefix + range_->value() + surround_.suffix;
						editor_->text(std::move(text));
						_m_draw_spins(spin_stated_);
					}
				}

				void _m_reset_text_area()
				{
					auto spins_r = _m_spins_area();
					if (spins_r.x == 0)
						editor_->text_area({});
					else
						editor_->text_area({ 2, 2, graph_->width() - spins_r.width - 2, spins_r.height - 2 });
				}

				::nana::rectangle _m_spins_area() const
				{
					auto size = API::window_size(editor_->window_handle());
					if (size.width > 18)
						return{ static_cast<int>(size.width - 16), 0, 16, size.height };
					
					return{ 0, 0, size.width, size.height };
				}

				buttons _m_where(const ::nana::point& pos) const
				{
					auto spins_r = _m_spins_area();
					if (spins_r.is_hit(pos))
					{
						if (pos.y < spins_r.y + static_cast<int>(spins_r.height / 2))
							return buttons::increase;

						return buttons::decrease;
					}
					return buttons::none;
				}

				void _m_draw_spins(buttons spins)
				{
					auto estate = API::element_state(editor_->window_handle());

					auto spin_r0 = _m_spins_area();
					spin_r0.height /= 2;

					auto spin_r1 = spin_r0;
					spin_r1.y += static_cast<int>(spin_r0.height);
					spin_r1.height = _m_spins_area().height - spin_r0.height;

					::nana::color bgcolor{ 3, 65, 140 };
					facade<element::arrow> arrow;
					facade<element::button> button;

					auto spin_state = (buttons::increase == spins ? estate : element_state::normal);
					button.draw(*graph_, bgcolor, colors::white, spin_r0, spin_state);
					spin_r0.x += 5;
					arrow.draw(*graph_, bgcolor, colors::white, spin_r0, spin_state);

					spin_state = (buttons::decrease == spins ? estate : element_state::normal);
					button.draw(*graph_, bgcolor, colors::white, spin_r1, spin_state);
					spin_r1.x += 5;
					arrow.direction(direction::south);
					arrow.draw(*graph_, bgcolor, colors::white, spin_r1, spin_state);
				}
			private:
				::nana::paint::graphics * graph_{nullptr};
				::nana::widgets::skeletons::text_editor * editor_{nullptr};
				buttons spin_stated_{ buttons::none };
				std::unique_ptr<range_interface> range_;
				::nana::timer timer_;

				struct surround_data
				{
					std::wstring prefix;
					std::wstring suffix;
				}surround_;
			};

			//class drawer
			drawer::drawer()
				: impl_(new implementation)
			{}

			drawer::~drawer()
			{
				delete impl_;
			}

			implementation* drawer::impl() const
			{
				return impl_;
			}

			//Overrides drawer_trigger
			void drawer::attached(widget_reference wdg, graph_reference graph)
			{
				impl_->attach(wdg, graph);
			}
			
			void drawer::refresh(graph_reference)
			{
				impl_->render();
			}

			void drawer::focus(graph_reference, const arg_focus&)
			{
				impl_->render();
				impl_->editor()->reset_caret();
				API::lazy_refresh();
			}
			
			void drawer::mouse_wheel(graph_reference, const arg_wheel& arg)
			{
				impl_->mouse_wheel(arg.upwards);
				impl_->editor()->reset_caret();
				API::lazy_refresh();
			}

			void drawer::mouse_down(graph_reference, const arg_mouse& arg)
			{
				if (impl_->mouse_button(arg, true))
					API::lazy_refresh();
			}

			void drawer::mouse_up(graph_reference, const arg_mouse& arg)
			{
				if (impl_->mouse_button(arg, false))
					API::lazy_refresh();
			}

			void drawer::mouse_move(graph_reference, const arg_mouse& arg)
			{
				if (impl_->mouse_move(arg.left_button, arg.pos))
					API::lazy_refresh();
			}

			void drawer::mouse_leave(graph_reference, const arg_mouse&)
			{
				impl_->render();
				API::lazy_refresh();
			}
			
		}
	}//end namespace drawerbase

	spinbox::spinbox()
	{}

	spinbox::spinbox(window wd, bool visible)
	{
		this->create(wd, visible);
	}

	spinbox::spinbox(window wd, const nana::rectangle& r, bool visible)
	{
		this->create(wd, r, visible);
	}

	void spinbox::range(int begin, int last, int step)
	{
		using namespace drawerbase::spinbox;
		get_drawer_trigger().impl()->set_range(std::unique_ptr<range_interface>(new range_numeric<int>(begin, last, step)));
		API::refresh_window(handle());
	}

	void spinbox::range(double begin, double last, double step)
	{
		using namespace drawerbase::spinbox;
		get_drawer_trigger().impl()->set_range(std::unique_ptr<range_interface>(new range_numeric<double>(begin, last, step)));
		API::refresh_window(handle());
	}

	void spinbox::range(std::initializer_list<std::string> steps_utf8)
	{
		using namespace drawerbase::spinbox;
		get_drawer_trigger().impl()->set_range(std::unique_ptr<range_interface>(new range_text(steps_utf8)));
		API::refresh_window(handle());
	}

	void spinbox::range(std::initializer_list<std::wstring> steps)
	{
		using namespace drawerbase::spinbox;
		get_drawer_trigger().impl()->set_range(std::unique_ptr<range_interface>(new range_text(steps)));
		API::refresh_window(handle());
	}

	void spinbox::qualify(std::wstring prefix, std::wstring suffix)
	{
		get_drawer_trigger().impl()->qualify(std::move(prefix), std::move(suffix));
	}

	void spinbox::qualify(const std::string & prefix_utf8, const std::string& suffix_utf8)
	{
		qualify(static_cast<std::wstring>(::nana::charset(prefix_utf8, ::nana::unicode::utf8)), static_cast<std::wstring>(::nana::charset(suffix_utf8, ::nana::unicode::utf8)));
	}

	::nana::string spinbox::_m_caption() const
	{
		internal_scope_guard lock;
		auto editor = get_drawer_trigger().impl()->editor();
		return (editor ? editor->text() : nana::string());
	}

	void spinbox::_m_caption(::nana::string&& text)
	{
		internal_scope_guard lock;
		auto editor = get_drawer_trigger().impl()->editor();
		if (editor)
		{
			editor->text(std::move(text));
			API::refresh_window(*this);
		}
	}
}//end namespace nana