import { Container } from "./components/container";
import { Hero } from "./components/hero";
import { Navbar } from "./components/navbar";

export default function Home() {
  return (
    <div className="layout">
      <Container>
        <Navbar />
        <Hero />
      </Container>
    </div>
  );
}
