import Hero from './components/Hero';
import Features from './components/Features';
import Performance from './components/Performance';
import Architecture from './components/Architecture';
import TechStack from './components/TechStack';
import Changelog from './components/Changelog';
import LiveDemo from './components/LiveDemo';
import Footer from './components/Footer';

function App() {
  return (
    <div className="min-h-screen bg-white">
      <Hero />
      <Features />
      <Performance />
      <Architecture />
      <TechStack />
      <Changelog />
      <LiveDemo />
      <Footer />
    </div>
  );
}

export default App;
