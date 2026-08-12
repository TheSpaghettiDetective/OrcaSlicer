#include "NativeUISpike.hpp"

#include "GUI_App.hpp"
#include "Widgets/WebView.hpp"
#include "libslic3r/Utils.hpp"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <utility>

#include <boost/log/trivial.hpp>

#include <wx/button.h>
#include <wx/filesys.h>
#include <wx/filename.h>
#include <wx/listbox.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/tglbtn.h>
#include <wx/timer.h>
#include <wx/webview.h>

namespace Slic3r::GUI {

namespace {

constexpr int BRIDGE_VERSION = 1;

wxString display_name(const std::string& name, const wxString& fallback)
{
    const wxString converted = from_u8(name);
    return converted.empty() ? fallback : converted;
}

} // namespace

class NativeUISpikeShell::Impl
{
public:
    Impl(wxWindow* parent, Callbacks callbacks)
        : m_callbacks(std::move(callbacks))
        , m_top_panel(new wxPanel(parent, wxID_ANY))
        , m_left_panel(new wxPanel(parent, wxID_ANY))
        , m_agent_panel(new wxPanel(parent, wxID_ANY))
    {
        build_top_panel();
        build_left_panel();
        build_agent_panel();
        m_stream_timer.Bind(wxEVT_TIMER, [this](wxTimerEvent&) { emit_next_stream_chunk(); });
    }

    ~Impl() { m_stream_timer.Stop(); }

    void set_project_state(ProjectState state)
    {
        m_project_state            = std::move(state);
        m_updating_native_controls = true;

        m_plate_list->Clear();
        for (size_t index = 0; index < m_project_state.plates.size(); ++index)
            m_plate_list->Append(display_name(m_project_state.plates[index], wxString::Format("Plate %zu", index + 1)));
        if (m_project_state.selected_plate >= 0 && m_project_state.selected_plate < static_cast<int>(m_plate_list->GetCount()))
            m_plate_list->SetSelection(m_project_state.selected_plate);

        m_object_list->Clear();
        for (size_t index = 0; index < m_project_state.objects.size(); ++index)
            m_object_list->Append(display_name(m_project_state.objects[index], wxString::Format("Object %zu", index + 1)));
        if (m_project_state.selected_object >= 0 && m_project_state.selected_object < static_cast<int>(m_object_list->GetCount()))
            m_object_list->SetSelection(m_project_state.selected_object);

        m_empty_objects->Show(m_project_state.objects.empty());
        m_left_panel->Layout();
        m_updating_native_controls = false;
        send_project_state();
    }

    void set_current_view(bool preview)
    {
        m_updating_native_controls = true;
        m_prepare_button->SetValue(!preview);
        m_preview_button->SetValue(preview);
        m_updating_native_controls = false;
    }

    void      set_left_pane_shown(bool shown) { m_toggle_left_button->SetLabel(shown ? "Hide list" : "Show list"); }
    wxWindow* top_panel() const { return m_top_panel; }
    wxWindow* left_panel() const { return m_left_panel; }
    wxWindow* agent_panel() const { return m_agent_panel; }

private:
    void build_top_panel()
    {
        auto*  sizer = new wxBoxSizer(wxHORIZONTAL);
        auto*  title = new wxStaticText(m_top_panel, wxID_ANY, "Native UI composition spike");
        wxFont font  = title->GetFont();
        font.SetWeight(wxFONTWEIGHT_BOLD);
        title->SetFont(font);

        m_toggle_left_button = new wxButton(m_top_panel, wxID_ANY, "Hide list", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_prepare_button     = new wxToggleButton(m_top_panel, wxID_ANY, "Prepare", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_preview_button     = new wxToggleButton(m_top_panel, wxID_ANY, "Preview", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_slice_button       = new wxButton(m_top_panel, wxID_ANY, "Slice", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_prepare_button->SetValue(true);

        sizer->Add(m_toggle_left_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, m_top_panel->FromDIP(12));
        sizer->Add(title, 0, wxALIGN_CENTER_VERTICAL);
        sizer->AddStretchSpacer();
        sizer->Add(m_prepare_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, m_top_panel->FromDIP(4));
        sizer->Add(m_preview_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, m_top_panel->FromDIP(12));
        sizer->Add(m_slice_button, 0, wxALIGN_CENTER_VERTICAL);
        m_top_panel->SetSizer(sizer);

        m_toggle_left_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            if (m_callbacks.toggle_left_pane)
                m_callbacks.toggle_left_pane();
        });
        m_prepare_button->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) {
            if (!m_updating_native_controls && m_callbacks.show_prepare)
                m_callbacks.show_prepare();
        });
        m_preview_button->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) {
            if (!m_updating_native_controls && m_callbacks.show_preview)
                m_callbacks.show_preview();
        });
        m_slice_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            if (m_callbacks.slice)
                m_callbacks.slice();
        });
    }

    void build_left_panel()
    {
        const int pad          = m_left_panel->FromDIP(12);
        auto*     sizer        = new wxBoxSizer(wxVERTICAL);
        auto*     heading      = new wxStaticText(m_left_panel, wxID_ANY, "Project");
        wxFont    heading_font = heading->GetFont();
        heading_font.SetWeight(wxFONTWEIGHT_BOLD);
        heading->SetFont(heading_font);

        sizer->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, pad);
        sizer->Add(new wxStaticText(m_left_panel, wxID_ANY, "Plates"), 0, wxLEFT | wxRIGHT | wxTOP, pad);
        m_plate_list = new wxListBox(m_left_panel, wxID_ANY);
        sizer->Add(m_plate_list, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);
        sizer->Add(new wxStaticLine(m_left_panel), 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);
        sizer->Add(new wxStaticText(m_left_panel, wxID_ANY, "Objects"), 0, wxLEFT | wxRIGHT | wxTOP, pad);
        m_object_list = new wxListBox(m_left_panel, wxID_ANY);
        sizer->Add(m_object_list, 1, wxEXPAND | wxALL, pad);
        m_empty_objects = new wxStaticText(m_left_panel, wxID_ANY, "Load a model to exercise two-way selection.");
        m_empty_objects->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
        sizer->Add(m_empty_objects, 0, wxLEFT | wxRIGHT | wxBOTTOM, pad);
        m_left_panel->SetSizer(sizer);

        m_plate_list->Bind(wxEVT_LISTBOX, [this](wxCommandEvent& event) {
            if (!m_updating_native_controls && m_callbacks.select_plate)
                m_callbacks.select_plate(event.GetSelection());
        });
        m_object_list->Bind(wxEVT_LISTBOX, [this](wxCommandEvent& event) {
            if (!m_updating_native_controls && m_callbacks.select_object)
                m_callbacks.select_object(event.GetSelection());
        });
    }

    void build_agent_panel()
    {
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        m_webview   = WebView::CreateWebView(m_agent_panel, wxEmptyString);
        sizer->Add(m_webview, 1, wxEXPAND);
        m_agent_panel->SetSizer(sizer);

        m_webview->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED, [this](wxWebViewEvent& event) { handle_script_message(event.GetString()); });
        m_webview->Bind(wxEVT_WEBVIEW_LOADED, [this](wxWebViewEvent&) {
            m_webview_ready = true;
            send_project_state();
        });

        wxFileName page(from_u8(resources_dir()), wxEmptyString);
        page.AppendDir("web");
        page.AppendDir("native-ui-spike");
        page.AppendDir("dist");
        page.SetFullName("index.html");
        if (page.FileExists()) {
            m_webview->LoadURL(wxFileSystem::FileNameToURL(page));
        } else {
            BOOST_LOG_TRIVIAL(error) << "Native UI spike page is missing: " << into_u8(page.GetFullPath());
            m_webview->SetPage("<html><body><h3>Native UI spike asset missing</h3></body></html>", wxEmptyString);
        }
    }

    void handle_script_message(const wxString& message)
    {
        const nlohmann::json envelope = nlohmann::json::parse(into_u8(message), nullptr, false);
        if (envelope.is_discarded() || envelope.value("version", 0) != BRIDGE_VERSION || envelope.value("kind", "") != "command" ||
            !envelope.contains("command") || !envelope["command"].is_object()) {
            send_bridge_error("invalid_command", "The native bridge rejected a malformed command.");
            return;
        }

        const nlohmann::json& command = envelope["command"];
        const std::string     type    = command.value("type", "");
        if (type == "project.request_state") {
            send_project_state();
            return;
        }
        if (type != "chat.send" || !command.contains("text") || !command["text"].is_string()) {
            send_bridge_error("unsupported_command", "This command is not supported by the Spike 1 bridge.");
            return;
        }

        const std::string text = command["text"].get<std::string>();
        if (text.empty() || text.size() > 2000) {
            send_bridge_error("invalid_chat_text", "Chat text must contain between 1 and 2000 bytes.");
            return;
        }
        start_streaming_response(text);
    }

    void start_streaming_response(const std::string& prompt)
    {
        m_stream_timer.Stop();
        m_stream_chunks.clear();
        m_next_stream_chunk = 0;
        ++m_response_sequence;
        m_active_response_id = "mock-" + std::to_string(m_response_sequence);

        const std::string response =
            "This is a simulated local response for the native UI composition spike. The WebView is receiving typed chunks from C++ while "
            "the real OrcaSlicer canvas remains interactive. Keep orbiting the model, collapse and restore the project list, resize the "
            "window, "
            "and switch between Prepare and Preview while this response arrives. The response is deliberately long enough to expose "
            "obvious "
            "focus, resize, scroll anchoring, or rendering problems at the native and WebView boundary. C++ remains the only owner of "
            "project "
            "state; this local React interface only submits commands and renders versioned events. No agent service, account, settings, or "
            "MCP "
            "connection is involved in this development-only demonstration. Your prompt began: " +
            prompt.substr(0, std::min<size_t>(prompt.size(), 80));
        std::istringstream words(response);
        std::string        word;
        while (words >> word)
            m_stream_chunks.emplace_back(word + " ");

        send_event({{"type", "chat.response.started"}, {"responseId", m_active_response_id}});
        m_stream_timer.Start(45);
    }

    void emit_next_stream_chunk()
    {
        if (m_next_stream_chunk >= m_stream_chunks.size()) {
            m_stream_timer.Stop();
            send_event({{"type", "chat.response.completed"}, {"responseId", m_active_response_id}});
            return;
        }
        send_event(
            {{"type", "chat.response.chunk"}, {"responseId", m_active_response_id}, {"text", m_stream_chunks[m_next_stream_chunk++]}});
    }

    void send_project_state()
    {
        nlohmann::json event = {{"type", "project.state"},
                                {"plates", m_project_state.plates},
                                {"objects", m_project_state.objects},
                                {"selectedPlate", m_project_state.selected_plate},
                                {"selectedObject", m_project_state.selected_object}};
        send_event(std::move(event));
    }

    void send_bridge_error(const std::string& code, const std::string& message)
    {
        send_event({{"type", "bridge.error"}, {"code", code}, {"message", message}});
    }

    void send_event(nlohmann::json event)
    {
        if (!m_webview_ready)
            return;
        const nlohmann::json envelope = {{"version", BRIDGE_VERSION}, {"kind", "event"}, {"event", std::move(event)}};
        WebView::RunScript(m_webview, "window.nativeUiSpikeReceive(" + from_u8(envelope.dump()) + ");");
    }

    Callbacks    m_callbacks;
    ProjectState m_project_state;

    wxPanel*        m_top_panel          = nullptr;
    wxPanel*        m_left_panel         = nullptr;
    wxPanel*        m_agent_panel        = nullptr;
    wxButton*       m_toggle_left_button = nullptr;
    wxToggleButton* m_prepare_button     = nullptr;
    wxToggleButton* m_preview_button     = nullptr;
    wxButton*       m_slice_button       = nullptr;
    wxListBox*      m_plate_list         = nullptr;
    wxListBox*      m_object_list        = nullptr;
    wxStaticText*   m_empty_objects      = nullptr;
    wxWebView*      m_webview            = nullptr;

    wxTimer                  m_stream_timer;
    std::vector<std::string> m_stream_chunks;
    size_t                   m_next_stream_chunk = 0;
    unsigned int             m_response_sequence = 0;
    std::string              m_active_response_id;
    bool                     m_webview_ready            = false;
    bool                     m_updating_native_controls = false;
};

NativeUISpikeShell::NativeUISpikeShell(wxWindow* parent, Callbacks callbacks) : m_impl(std::make_unique<Impl>(parent, std::move(callbacks)))
{}
NativeUISpikeShell::~NativeUISpikeShell() = default;

wxWindow* NativeUISpikeShell::top_panel() const { return m_impl->top_panel(); }
wxWindow* NativeUISpikeShell::left_panel() const { return m_impl->left_panel(); }
wxWindow* NativeUISpikeShell::agent_panel() const { return m_impl->agent_panel(); }
void      NativeUISpikeShell::set_project_state(ProjectState state) { m_impl->set_project_state(std::move(state)); }
void      NativeUISpikeShell::set_current_view(bool preview) { m_impl->set_current_view(preview); }
void      NativeUISpikeShell::set_left_pane_shown(bool shown) { m_impl->set_left_pane_shown(shown); }

bool native_ui_spike_enabled()
{
    const char* value = std::getenv("ORCA_NATIVE_UI_SPIKE");
    return value != nullptr && (std::string(value) == "1" || std::string(value) == "true");
}

} // namespace Slic3r::GUI
