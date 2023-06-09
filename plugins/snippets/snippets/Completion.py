from gi.repository import GObject, Ctk, CtkSource, Lapiz

from .Library import Library
from .LanguageManager import get_language_manager
from .Snippet import Snippet

class Proposal(GObject.Object, CtkSource.CompletionProposal):
    __gtype_name__ = "LapizSnippetsProposal"

    def __init__(self, snippet):
        super(Proposal, self).__init__()
        self._snippet = Snippet(snippet)

    def snippet(self):
        return self._snippet.data

    # Interface implementation
    def do_get_markup(self):
        return self._snippet.display()

    def do_get_info(self):
        return self._snippet.data['text']

class Provider(GObject.Object, CtkSource.CompletionProvider):
    __gtype_name__ = "LapizSnippetsProvider"

    def __init__(self, name, language_id, handler):
        super(Provider, self).__init__()

        self.name = name
        self.info_widget = None
        self.proposals = []
        self.language_id = language_id
        self.handler = handler
        self.info_widget = None
        self.mark = None

        theme = Ctk.IconTheme.get_default()
        f, w, h = Ctk.icon_size_lookup(Ctk.IconSize.MENU)

        try:
            self.icon = theme.load_icon("format-justify-left", w, 0)
        except:
            self.icon = None

    def __del__(self):
        if self.mark:
            self.mark.get_buffer().delete_mark(self.mark)

    def set_proposals(self, proposals):
        self.proposals = proposals

    def mark_position(self, it):
        if not self.mark:
            self.mark = it.get_buffer().create_mark(None, it, True)
        else:
            self.mark.get_buffer().move_mark(self.mark, it)

    def get_word(self, context):
        (valid_context, it) = context.get_iter()
        if not valid_context:
            return None

        if it.starts_word() or it.starts_line() or not it.ends_word():
            return None

        start = it.copy()

        if start.backward_word_start():
            self.mark_position(start)
            return start.get_text(it)
        else:
            return None

    def do_get_start_iter(self, context, proposal):
        if not self.mark or self.mark.get_deleted():
            return (False, None)

        return (True, self.mark.get_buffer().get_iter_at_mark(self.mark))

    def do_match(self, context):
        return True

    def get_proposals(self, word):
        if self.proposals:
            proposals = self.proposals
        else:
            proposals = Library().get_snippets(None)

            if self.language_id:
                proposals += Library().get_snippets(self.language_id)

        # Filter based on the current word
        if word:
            proposals = (x for x in proposals if x['tag'].startswith(word))

        return [Proposal(x) for x in proposals]

    def do_populate(self, context):
        proposals = self.get_proposals(self.get_word(context))
        context.add_proposals(self, proposals, True)

    def do_get_name(self):
        return self.name

    def do_activate_proposal(self, proposal, piter):
        return self.handler(proposal, piter)

    def do_get_info_widget(self, proposal):
        if not self.info_widget:
            view = Lapiz.View.new_with_buffer(Lapiz.Document())
            manager = get_language_manager()

            lang = manager.get_language('snippets')
            view.get_buffer().set_language(lang)

            sw = Ctk.ScrolledWindow()
            sw.add(view)
            sw.show_all()

            # Fixed size
            sw.set_size_request(300, 200)

            self.info_view = view
            self.info_widget = sw

        return self.info_widget

    def do_update_info(self, proposal, info):
        buf = self.info_view.get_buffer()

        buf.set_text(proposal.get_info())
        buf.move_mark(buf.get_insert(), buf.get_start_iter())
        buf.move_mark(buf.get_selection_bound(), buf.get_start_iter())
        self.info_view.scroll_to_iter(buf.get_start_iter(), 0.0, False, 0.5, 0.5)

    def do_get_icon(self):
        return self.icon

    def do_get_activation(self):
        return CtkSource.CompletionActivation.USER_REQUESTED

class Defaults(GObject.Object, CtkSource.CompletionProvider):
    __gtype_name__ = "LapizSnippetsDefaultsProvider"

    def __init__(self, handler):
        GObject.Object.__init__(self)

        self.handler = handler
        self.proposals = []

    def set_defaults(self, defaults):
        self.proposals = []

        for d in defaults:
            self.proposals.append(CtkSource.CompletionItem.new(d, d, None, None))

    def do_get_name(self):
        return ""

    def do_activate_proposal(self, proposal, piter):
        return self.handler(proposal, piter)

    def do_populate(self, context):
        context.add_proposals(self, self.proposals, True)

    def do_get_activation(self):
        return CtkSource.CompletionActivation.USER_REQUESTED

# ex:ts=4:et:
