namespace cogs_editable_automation{

    /// This reads /config/automation.json and creates approproiate bindings.
    /// When a global file change event happens, it will update them.
    void begin();

    bool started = false;
    /// Adds editing UI for the web.
   // void setupWebEditing();
}
