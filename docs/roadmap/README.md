# UnrealEngineCog Interactive Roadmap

An interactive roadmap visualization for the UnrealEngineCog cognitive architecture evolution project.

## Features

- **Interactive Timeline**: Visual representation of implementation phases
- **Progress Tracking**: Real-time progress metrics and milestone tracking
- **Responsive Design**: Works seamlessly across desktop and mobile devices
- **Dark Theme**: Modern dark UI with cyan/teal accent colors
- **Module Visualization**: Detailed view of system modules and dependencies

## Technology Stack

- **React 18**: Modern React with hooks
- **Vite**: Fast build tool and dev server
- **React Router**: Client-side routing
- **CSS3**: Custom styling with CSS variables

## Development

### Prerequisites

- Node.js 20.x or higher
- npm 10.x or higher

### Installation

```bash
cd docs/roadmap
npm install
```

### Running Locally

```bash
npm run dev
```

The application will be available at `http://localhost:3000`

### Building for Production

```bash
npm run build
```

The built files will be in the `dist/` directory.

## Deployment

### GitHub Pages

This project is configured to deploy automatically to GitHub Pages via GitHub Actions.

**Setup:**

1. Enable GitHub Pages in repository settings
2. Set source to "GitHub Actions"
3. Push changes to the `main` branch
4. The workflow will automatically build and deploy

**Manual Deployment:**

```bash
npm run build
# Deploy the dist/ directory to GitHub Pages
```

### Manus Deployment

The project is also deployed to Manus at:
https://ue-cog-roadmap-sefgqbzw.manus.space/

## Project Structure

```
docs/roadmap/
├── public/              # Static assets
├── src/
│   ├── components/      # React components
│   │   ├── Roadmap.jsx
│   │   ├── Progress.jsx
│   │   ├── Modules.jsx
│   │   └── MetaModel.jsx
│   ├── data/           # Data files
│   │   └── roadmapData.js
│   ├── styles/         # CSS files
│   │   ├── index.css
│   │   ├── App.css
│   │   └── Roadmap.css
│   ├── App.jsx         # Main App component
│   └── main.jsx        # Entry point
├── index.html          # HTML template
├── vite.config.js      # Vite configuration
└── package.json        # Dependencies
```

## Roadmap Data

The roadmap data is stored in `src/data/roadmapData.js` and can be easily updated to reflect project progress.

## Contributing

1. Update roadmap data in `src/data/roadmapData.js`
2. Test locally with `npm run dev`
3. Commit and push to trigger automatic deployment

## License

Part of the UnrealEngineCog project.
