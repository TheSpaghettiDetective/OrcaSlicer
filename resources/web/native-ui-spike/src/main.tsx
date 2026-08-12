import React, { FormEvent, useEffect, useRef, useState } from "react";
import { createRoot } from "react-dom/client";
import "./styles.css";

type Command =
  | { type: "project.request_state" }
  | { type: "chat.send"; text: string };

type ProjectStateEvent = {
  type: "project.state";
  plates: string[];
  objects: string[];
  selectedPlate: number;
  selectedObject: number;
};

type AgentEvent =
  | ProjectStateEvent
  | { type: "chat.response.started"; responseId: string }
  | { type: "chat.response.chunk"; responseId: string; text: string }
  | { type: "chat.response.completed"; responseId: string }
  | { type: "bridge.error"; code: string; message: string };

type EventEnvelope = { version: 1; kind: "event"; event: AgentEvent };
type Message = { id: string; role: "user" | "agent" | "system"; text: string; streaming?: boolean };

declare global {
  interface Window {
    wx?: { postMessage: (payload: string) => void };
    nativeUiSpikeReceive?: (envelope: EventEnvelope) => void;
  }
}

function sendCommand(command: Command) {
  window.wx?.postMessage(JSON.stringify({ version: 1, kind: "command", command }));
}

function App() {
  const [messages, setMessages] = useState<Message[]>([
    {
      id: "welcome",
      role: "agent",
      text: "This local mock streams through the typed C++ bridge. Try orbiting or resizing the real viewport while it responds.",
    },
  ]);
  const [project, setProject] = useState<ProjectStateEvent | null>(null);
  const [input, setInput] = useState("");
  const transcript = useRef<HTMLDivElement>(null);

  useEffect(() => {
    window.nativeUiSpikeReceive = (envelope) => {
      if (envelope.version !== 1 || envelope.kind !== "event") return;
      const event = envelope.event;
      if (event.type === "project.state") {
        setProject(event);
      } else if (event.type === "chat.response.started") {
        setMessages((current) => [...current, { id: event.responseId, role: "agent", text: "", streaming: true }]);
      } else if (event.type === "chat.response.chunk") {
        setMessages((current) =>
          current.map((message) =>
            message.id === event.responseId ? { ...message, text: message.text + event.text } : message,
          ),
        );
      } else if (event.type === "chat.response.completed") {
        setMessages((current) =>
          current.map((message) => (message.id === event.responseId ? { ...message, streaming: false } : message)),
        );
      } else if (event.type === "bridge.error") {
        setMessages((current) => [
          ...current,
          { id: `error-${Date.now()}`, role: "system", text: `${event.code}: ${event.message}` },
        ]);
      }
    };
    sendCommand({ type: "project.request_state" });
    return () => {
      delete window.nativeUiSpikeReceive;
    };
  }, []);

  useEffect(() => {
    const element = transcript.current;
    if (element) element.scrollTop = element.scrollHeight;
  }, [messages]);

  const submit = (event: FormEvent) => {
    event.preventDefault();
    const text = input.trim();
    if (!text) return;
    setMessages((current) => [...current, { id: `user-${Date.now()}`, role: "user", text }]);
    setInput("");
    sendCommand({ type: "chat.send", text });
  };

  return (
    <main>
      <header>
        <div>
          <h1>Agent</h1>
          <p>Local Spike 1 UI</p>
        </div>
        <span className="status">Mock</span>
      </header>
      <div className="project-state" aria-live="polite">
        {project
          ? `${project.plates.length} plate${project.plates.length === 1 ? "" : "s"} · ${project.objects.length} object${project.objects.length === 1 ? "" : "s"}`
          : "Waiting for native project state…"}
      </div>
      <section className="transcript" ref={transcript} aria-label="Conversation">
        {messages.map((message) => (
          <article key={message.id} className={`message ${message.role}`}>
            <span>{message.role === "user" ? "You" : message.role === "agent" ? "Agent" : "Bridge"}</span>
            <p>{message.text}{message.streaming && <i className="cursor" />}</p>
          </article>
        ))}
      </section>
      <form onSubmit={submit}>
        <textarea
          aria-label="Message"
          placeholder="Ask the local mock…"
          value={input}
          maxLength={2000}
          onChange={(event) => setInput(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === "Enter" && !event.shiftKey) {
              event.preventDefault();
              event.currentTarget.form?.requestSubmit();
            }
          }}
        />
        <button type="submit" disabled={!input.trim()}>Send</button>
      </form>
    </main>
  );
}

createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);
