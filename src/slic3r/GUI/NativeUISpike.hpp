#ifndef slic3r_GUI_NativeUISpike_hpp_
#define slic3r_GUI_NativeUISpike_hpp_

#include <functional>
#include <memory>
#include <string>
#include <vector>

class wxWindow;

namespace Slic3r::GUI {

// Development-only shell for exercising the native/WebView boundary described
// in docs/native-ui-rewrite-plan.md. It is enabled with
// ORCA_NATIVE_UI_SPIKE=1 and deliberately owns no Orca project state.
class NativeUISpikeShell final
{
public:
    struct ProjectState
    {
        std::vector<std::string> plates;
        std::vector<std::string> objects;
        int                      selected_plate  = -1;
        int                      selected_object = -1;
    };

    struct Callbacks
    {
        std::function<void()>    toggle_left_pane;
        std::function<void()>    show_prepare;
        std::function<void()>    show_preview;
        std::function<void()>    slice;
        std::function<void(int)> select_plate;
        std::function<void(int)> select_object;
    };

    NativeUISpikeShell(wxWindow* parent, Callbacks callbacks);
    ~NativeUISpikeShell();

    NativeUISpikeShell(const NativeUISpikeShell&)            = delete;
    NativeUISpikeShell& operator=(const NativeUISpikeShell&) = delete;

    wxWindow* top_panel() const;
    wxWindow* left_panel() const;
    wxWindow* agent_panel() const;

    void set_project_state(ProjectState state);
    void set_current_view(bool preview);
    void set_left_pane_shown(bool shown);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

bool native_ui_spike_enabled();

} // namespace Slic3r::GUI

#endif // slic3r_GUI_NativeUISpike_hpp_
