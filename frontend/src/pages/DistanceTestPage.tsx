import { useState } from 'react'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Badge } from '@/components/ui/badge'
import { Play } from 'lucide-react'

interface TestPoint {
  r: number
  x: number
  result: 'pass' | 'fail' | 'pending'
  tripTime?: number
}

export default function DistanceTestPage() {
  const [testPoints, setTestPoints] = useState<TestPoint[]>([
    { r: 2.0, x: 4.0, result: 'pending' },
    { r: 5.0, x: 10.0, result: 'pending' },
  ])
  const [newR, setNewR] = useState('0')
  const [newX, setNewX] = useState('0')

  const handleAddPoint = () => {
    setTestPoints([...testPoints, { r: parseFloat(newR), x: parseFloat(newX), result: 'pending' }])
    setNewR('0')
    setNewX('0')
  }

  const handleRunTest = () => {
    setTestPoints(testPoints.map(p => ({ ...p, result: Math.random() > 0.3 ? 'pass' : 'fail', tripTime: 25 + Math.random() * 10 })))
  }

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-3xl font-bold tracking-tight">Distance 21 Test</h1>
        <p className="text-muted-foreground">
          Automated testing for distance protection relays
        </p>
      </div>

      <div className="grid gap-6 lg:grid-cols-2">
        <Card>
          <CardHeader><CardTitle>Test Points Configuration</CardTitle><CardDescription>Define R-X impedance points and fault types for testing</CardDescription></CardHeader>
          <CardContent className="space-y-4">
            <div className="grid gap-4 sm:grid-cols-2">
              <div className="space-y-2">
                <Label htmlFor="r">Resistance (Ω)</Label>
                <Input id="r" type="number" step="0.1" value={newR} onChange={(e) => setNewR(e.target.value)} />
              </div>
              <div className="space-y-2">
                <Label htmlFor="x">Reactance (Ω)</Label>
                <Input id="x" type="number" step="0.1" value={newX} onChange={(e) => setNewX(e.target.value)} />
              </div>
            </div>
            <Button onClick={handleAddPoint} className="w-full">Add Test Point</Button>
            <div className="space-y-2">
              {testPoints.map((pt, idx) => (
                <div key={idx} className="flex items-center justify-between p-2 border rounded text-sm">
                  <span className="font-mono">R={pt.r}Ω, X={pt.x}Ω</span>
                  <Badge variant={pt.result === 'pass' ? 'default' : pt.result === 'fail' ? 'destructive' : 'outline'}>{pt.result}</Badge>
                </div>
              ))}
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader><CardTitle>R-X Diagram</CardTitle><CardDescription>Impedance plane visualization</CardDescription></CardHeader>
          <CardContent>
            <div className="h-64 flex items-center justify-center border rounded-lg bg-muted/20">
              <p className="text-sm text-muted-foreground">R-X diagram with zones coming soon...</p>
            </div>
            <div className="mt-4">
              <Button onClick={handleRunTest} className="w-full"><Play className="mr-2 h-4 w-4" />Run Test</Button>
            </div>
          </CardContent>
        </Card>
      </div>
    </div>
  )
}
